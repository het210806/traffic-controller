🚦 Traffic Signal Controller Simulation (Multithreaded C Project)

This project is a multithreaded traffic signal controller simulation written in C using POSIX threads (pthreads).
It simulates how traffic lights operate at a four-lane junction, dynamically adjusting green light duration based on vehicle load and handling emergency interruptions in real time.

The project is designed to demonstrate core operating system and concurrent programming concepts such as threads, mutexes, condition variables, and event scheduling.

📌 Project Overview

The simulation models:

A 4-lane traffic intersection

Vehicles arriving at lanes over time

A traffic controller that decides which lane gets the green signal

Emergency events that temporarily halt normal traffic flow

All actions are logged to a file and printed on the console to clearly show how the system behaves step by step.

⚙️ Key Features

Event-driven simulation

Vehicle arrival events

Emergency trigger events

Dynamic green signal timing

Green light duration depends on vehicle count

Minimum and maximum green time limits

Emergency handling

All signals turn RED during emergency

Traffic resumes automatically after emergency clears

Multithreading

Separate threads for event scheduling and traffic control

Thread-safe logging

Logs written safely using mutex locks

🧠 Concepts Demonstrated

This project strongly reflects topics from Computer System Programming / Operating Systems:

Multithreading using pthread

Mutex locks for shared data protection

Condition variables for thread synchronization

Event scheduling and time simulation

Shared resource management

File I/O for logging

Real-time system behavior simulation
