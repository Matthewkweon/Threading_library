#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "semaphore.h"

#define THREAD_CNT 3

// Array of semaphores to control thread execution order
sem_t sem[THREAD_CNT];

// Modified count function that uses semaphores for ordered execution
void *count(void *arg) {
    unsigned long int c = (unsigned long int)arg;
    int thread_num = (int)(c / 10000000) - 1;  // Calculate thread number (0, 1, or 2)
    
    // Wait for previous thread to signal (except thread 0) UNCOMMENT all semaphore functions to see it working
    // if (thread_num > 0) {
    //     sem_wait(&sem[thread_num - 1]);
    // }
    
    printf("Thread %d (tid: 0x%x) starting to count...\n", 
           thread_num, (unsigned int)pthread_self());

    // Declare the loop variable outside to be compatible with older C standards
    int i;
    for (i = 0; i < c; i++) {
        if ((i % 1000) == 0) {
            printf("tid: 0x%x Just counted to %d of %ld\n",
                   (unsigned int)pthread_self(), i, c);
        }
    }
    
    printf("Thread %d (tid: 0x%x) completed counting.\n", 
           thread_num, (unsigned int)pthread_self());

    // Signal next thread that we're done
    // if (thread_num < THREAD_CNT - 1) {
    //     sem_post(&sem[thread_num]);
    // }
    
    return arg;
}

int main(int argc, char **argv) {
    pthread_t threads[THREAD_CNT];
    int i;
    unsigned long int cnt = 10000000;

    // Initialize the semaphores
    // sem_init(&sem[0], 0, 1);  // Only the first thread can start
    // for (i = 1; i < THREAD_CNT; i++) {
    //     sem_init(&sem[i], 0, 0);  // Other threads wait
    // }

    // Create THREAD_CNT threads
    for(i = 0; i < THREAD_CNT; i++) {                        
        pthread_create(&threads[i], NULL, count, (void *)((i+1)*cnt));
    }

    // Join all threads - UNCOMMENT TO MAKE SURE MAIN THREAD FINISHES LAST
    // for(i = 0; i < THREAD_CNT; i++) {
    //     int ret = pthread_join(threads[i], NULL);
    //     if (ret != 0) {
    //         fprintf(stderr, "Error: Invalid thread %d\n", i);
    //     }
    // }

    // // Clean up semaphores                        // uncomment for semaphore usage
    // for (i = 0; i < THREAD_CNT; i++) {
    //     sem_destroy(&sem[i]);
    // }
    count((void *)(cnt * (THREAD_CNT + 1)));


    return 0;
}
