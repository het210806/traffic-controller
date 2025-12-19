#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>     // for usleep
#include <stdbool.h>

#define MAX_EVENTS 1000
#define MAX_LINE 128

// ---------------------- DATA STRUCTURES -----------------------

typedef struct {
    int time;
    char type[16];
    int lane;
    int count;
} Event;

Event eventQueue[MAX_EVENTS];
int eventCount = 0;

int vehicleCount[4] = {0, 0, 0, 0};

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cvResume = PTHREAD_COND_INITIALIZER;

bool emergencyActive = false;
bool emergencyPaused = false;
bool resumeSignal = false;

FILE *logfile;

// ---------------------- LOGGING -----------------------

void logEvent(const char *msg) {
    pthread_mutex_lock(&logMutex);
    fprintf(logfile, "%s\n", msg);
    fflush(logfile);
    printf("%s\n", msg);
    pthread_mutex_unlock(&logMutex);
}

// ---------------------- EVENT LOADING -----------------------

void loadEvents() {
    FILE *fp = fopen("events.txt", "r");
    if (!fp) {
        logEvent("Could not open events.txt. No events loaded.");
        return;
    }

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || strlen(line) < 2)
            continue;

        Event e;
        char type[16];

        if (sscanf(line, "%d %s", &e.time, type) != 2)
            continue;

        strcpy(e.type, type);

        if (strcmp(type, "vehicle") == 0) {
            sscanf(line, "%d %s %d %d",
                   &e.time, e.type, &e.lane, &e.count);
        } else {
            e.lane = -1;
            e.count = 0;
        }

        eventQueue[eventCount++] = e;
    }

    fclose(fp);
}

// ---------------------- EVENT SCHEDULER -----------------------

void *eventScheduler(void *arg) {
    int currentTime = 0;

    for (int i = 0; i < eventCount; i++) {
        Event e = eventQueue[i];

        int waitMs = (e.time - currentTime) * 100;
        if (waitMs > 0)
            usleep(waitMs * 1000);

        currentTime = e.time;

        pthread_mutex_lock(&mtx);

        if (strcmp(e.type, "vehicle") == 0) {
            if (e.lane >= 0 && e.lane < 4) {
                vehicleCount[e.lane] += e.count;

                char msg[100];
                sprintf(msg, "Time %d: Added %d vehicles to lane %d, total = %d",
                        currentTime, e.count, e.lane, vehicleCount[e.lane]);
                logEvent(msg);
            }
        }
        else if (strcmp(e.type, "emergency") == 0) {
            emergencyActive = true;
            emergencyPaused = true;

            char msg1[80];
            sprintf(msg1, "Time %d: Emergency triggered!", currentTime);
            logEvent(msg1);

            // Start auto-clear thread
            pthread_t t;
            pthread_create(&t, NULL, (void *(*)(void *))({
                void* emergencyClearThread(void *unused) {
                    usleep(500 * 1000);  // 5 simulated seconds

                    pthread_mutex_lock(&mtx);
                    emergencyPaused = false;
                    emergencyActive = false;
                    resumeSignal = true;
                    pthread_cond_signal(&cvResume);
                    pthread_mutex_unlock(&mtx);

                    logEvent("Emergency auto-cleared (simulated).");
                    return NULL;
                }
                emergencyClearThread;
            }), NULL);
            pthread_detach(t);
        }

        pthread_mutex_unlock(&mtx);
    }

    return NULL;
}

// ---------------------- TRAFFIC CONTROLLER -----------------------

void *trafficController(void *arg) {
    const int minGreen = 10;
    const int maxGreen = 90;

    int currentLane = 0;
    int greenTimeLeft = 0;
    bool resumingGreenPhase = false;

    while (1) {
        pthread_mutex_lock(&mtx);

        bool vehiclesRemaining = false;
        for (int i = 0; i < 4; i++) {
            if (vehicleCount[i] > 0)
                vehiclesRemaining = true;
        }

        bool eventsDone = (eventCount == 0);

        if (!vehiclesRemaining && !emergencyActive && !emergencyPaused) {
            pthread_mutex_unlock(&mtx);
            logEvent("All vehicles cleared. Ending simulation.");
            break;
        }

        pthread_mutex_unlock(&mtx);

        // EMERGENCY PAUSE
        if (emergencyPaused) {
            logEvent("Emergency pause detected: all RED, waiting...");

            pthread_mutex_lock(&mtx);
            while (!resumeSignal)
                pthread_cond_wait(&cvResume, &mtx);
            resumeSignal = false;
            pthread_mutex_unlock(&mtx);

            logEvent("Controller resumed after emergency.");
            continue;
        }

        // Determine green time
        int greenTime = 0;

        pthread_mutex_lock(&mtx);
        if (resumingGreenPhase) {
            greenTime = greenTimeLeft;
            resumingGreenPhase = false;
        } else {
            greenTime = vehicleCount[currentLane];
            if (greenTime < minGreen) greenTime = minGreen;
            else if (greenTime > maxGreen) greenTime = maxGreen;
        }
        pthread_mutex_unlock(&mtx);

        char msg[100];
        sprintf(msg, "Lane %d GREEN for %d seconds", currentLane, greenTime);
        logEvent(msg);

        bool interrupted = false;

        for (int i = 0; i < greenTime; i++) {
            if (emergencyActive) {
                greenTimeLeft = greenTime - i;
                interrupted = true;

                char m2[90];
                sprintf(m2, "Emergency mid-green! Pausing with %d seconds left.",
                        greenTimeLeft);
                logEvent(m2);

                pthread_mutex_lock(&mtx);
                emergencyPaused = true;

                while (!resumeSignal)
                    pthread_cond_wait(&cvResume, &mtx);

                resumeSignal = false;
                pthread_mutex_unlock(&mtx);

                break;
            }

            pthread_mutex_lock(&mtx);
            if (vehicleCount[currentLane] > 0) {
                vehicleCount[currentLane]--;
                char m3[100];
                sprintf(m3, "Lane %d: Vehicle passed. Remaining: %d",
                        currentLane, vehicleCount[currentLane]);
                logEvent(m3);
            } else {
                char m4[100];
                sprintf(m4, "Lane %d: No vehicles at this tick.", currentLane);
                logEvent(m4);
            }
            pthread_mutex_unlock(&mtx);

            usleep(100 * 1000);
        }

        if (!interrupted) {
            greenTimeLeft = 0;
            resumingGreenPhase = false;

            int oldLane = currentLane;
            currentLane = (currentLane + 1) % 4;

            char m5[100];
            sprintf(m5, "Lane %d RED. Next lane: %d", oldLane, currentLane);
            logEvent(m5);
        } else {
            resumingGreenPhase = true;
        }
    }

    logEvent("Simulation complete.");
    return NULL;
}

// ---------------------- MAIN -----------------------

int main() {
    logfile = fopen("traffic_log.txt", "w");
    if (!logfile) {
        printf("Cannot open traffic_log.txt\n");
        return 1;
    }

    logEvent("Traffic simulation started");

    loadEvents();

    pthread_t schedulerThread, controllerThread;

    pthread_create(&schedulerThread, NULL, eventScheduler, NULL);
    pthread_create(&controllerThread, NULL, trafficController, NULL);

    pthread_join(schedulerThread, NULL);
    pthread_join(controllerThread, NULL);

    logEvent("All threads finished. Exiting.");

    fclose(logfile);
    return 0;
}

