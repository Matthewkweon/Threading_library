#Custom Threads Library with Synchronization and Scheduling
This project implements a custom thread library in C, providing core thread management functions (pthread_create, pthread_join, and pthread_exit) alongside basic semaphore-based synchronization (sem_init, sem_wait, sem_post, and sem_destroy). The project is built to support multithreading through a round-robin scheduler, thread blocking and unblocking, and an implementation of semaphores for thread synchronization.

Table of Contents
Project Overview
Features and Implementation
Thread Management Functions
Scheduling and Context Switching
Semaphore Implementation
Synchronization Mechanisms
Challenges Faced
Usage
File Structure
Project Overview
This custom thread library is designed to simulate multithreading at the user level, implementing a subset of POSIX thread (pthreads) and semaphore functions. This project includes custom definitions for context switching, round-robin scheduling, thread synchronization, and memory management. By mimicking the pthread library, this project showcases a foundational understanding of thread scheduling and management at a low level.

Features and Implementation
Thread Management Functions
The main thread management functions provided are:

pthread_create: Creates a new thread with its own stack, program counter, and state.
pthread_join: Allows one thread to wait for another thread to finish, managing the transition of thread states and cleaning up resources.
pthread_exit: Signals that a thread has completed execution and manages resource deallocation and state changes.
Key Points:

Each thread has a unique myTCB (Thread Control Block) that holds information about the thread's stack, status, and context.
The pthread_join function includes checks to prevent threads from joining themselves, handles errors if a thread attempts to join multiple times, and uses a blocking mechanism to wait until the joined thread finishes.
Threads store exit statuses in a global array, allowing for retrieval by pthread_join.
Scheduling and Context Switching
This library implements a basic round-robin scheduler, which switches between threads at regular intervals:

schedule: Invoked by a signal handler to switch between threads using the setjmp/longjmp functions for context switching.
Signal-based Preemption: Utilizes SIGALRM to set a timer that triggers preemption and calls the schedule function for a context switch.
Round-robin: Threads are added to a ready queue (ready_queue), and the scheduler cycles through each runnable thread. Blocked threads are bypassed in the round-robin rotation.
Semaphore Implementation
The library provides basic semaphore functionality with:

sem_init: Initializes a semaphore with a specified value.
sem_wait: Waits on the semaphore, decrementing its count. If the count is zero, the calling thread is blocked and added to a semaphore queue.
sem_post: Increments the semaphore count, unblocking the next thread in the semaphore queue if available.
sem_destroy: Cleans up the semaphore, ensuring no threads are waiting on it.
Semaphore Details:

Each semaphore keeps a queue of blocked threads.
Threads that are blocked in sem_wait are re-added to the ready queue when sem_post is called, allowing other threads to proceed in a controlled order.
Synchronization Mechanisms
To prevent race conditions, this library implements:

Locks and Unlocks: Wrapper functions for sigprocmask to block and unblock SIGALRM signals around critical sections.
Blocking and Unblocking Threads: In pthread_join, sem_wait, and other functions, threads can be put in a BLOCKED state until the required resources are available.
Exit Status Management: Threads’ exit statuses are stored in a global array, accessible by the joining thread.
Challenges Faced
Implementing the pthread_join function presented unique challenges in managing synchronization between threads. The function had to ensure a thread could not join itself or attempt to join a thread that was already joined, requiring checks and a robust handling of blocking states. Synchronizing threads using semaphores introduced additional complexity in maintaining correct counts and managing blocked queues. Implementing a signal-driven preemptive scheduler with SIGALRM and schedule required meticulous attention to the timing and context switching mechanisms to avoid race conditions. Finally, managing memory deallocation for thread stacks and ensuring that resources were cleaned up in pthread_exit without causing segmentation faults posed significant debugging challenges.

Usage
To compile and run this project, use:

bash
Copy code
gcc -o threads threads.c -lpthread
./threads
File Structure
threads.c: Main implementation file, containing all thread functions, semaphore functions, and the scheduler.
ec440threads.h: Header file with definitions for thread and semaphore structures, constants, and function prototypes.
semaphore.h: Header for POSIX-like semaphore definitions using a union for alignment.
Function Details
pthread_create: Allocates a new stack for the thread, initializes its context, and adds it to the ready queue. Handles memory management for the stack and defines a start routine.

pthread_join: Manages joining by blocking the calling thread until the specified thread finishes. Checks to prevent self-joining and handles multiple joins on the same thread.

pthread_exit: Cleans up resources, updates the status to EXITED, and unblocks any thread waiting for the exit status.

Semaphores (sem_init, sem_wait, sem_post, sem_destroy): These functions provide mutual exclusion and blocking/waiting capabilities, critical for ensuring safe access to shared resources between threads.
