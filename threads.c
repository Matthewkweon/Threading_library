#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include "ec440threads.h"
#include "semaphore.h"
#include <stdbool.h>


#define MAX_THREADS 128
// #define X 50

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

int first_time = 1;
int all_threads_done = 0;
int current_thread;
volatile int thread_yielded = 0;


void* exit_status[MAX_THREADS] = {NULL};
int joined_threads[MAX_THREADS] = {-1};
int threads_joined[MAX_THREADS] = {0};

void lock(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void unlock(){
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void pthread_exit_wrapper()
{
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

void schedule();


typedef struct{
    void* stack;
    // int program_counter;
    int thread_id;
    // int registers[13];
    char status[10];
    jmp_buf context;
    // void *(*start_routine)(void *); 
    // void *arg;
} myTCB;

typedef struct QueueNode {
    myTCB *thread;             
    struct QueueNode *next;      
} QueueNode;

typedef struct {
    QueueNode *front;
    QueueNode *back;
} myQ;

void init_queue(myQ *q) {
    q->front = NULL;
    q->back = NULL;
}

void enqueue(myQ *q, myTCB *thread) {
    QueueNode *new_node =(QueueNode *)malloc(sizeof(QueueNode));
    new_node->thread = thread;
    new_node->next = NULL;
    if (q->front != NULL) {
        q->back->next = new_node;
        q->back = new_node;
    } else {
        q->front = new_node;
        q->back = new_node;
    }
}

myTCB *dequeue(myQ *q) {
    if (q->front == NULL) {
        all_threads_done = 1;
        // printf("All threads have exited\n");
        return NULL;
    }
    // printf("Dequeue\n");
    QueueNode *temp = q->front;
    myTCB *thread = temp->thread;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->back = NULL;
    }
    free(temp);
    // printf("Dequeue done\n");
    return thread;
}



myTCB threads[MAX_THREADS];
int thread_count = 0;
myQ ready_queue;

void signal_handler(int sig){
    // printf("Debug: Signal handler called. Signal: %d\n", sig);
    // sigset_t mask, prev_mask;
    // sigfillset(&mask);
    // sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    if (ready_queue.front && !all_threads_done)
    {schedule();}
    // sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    
    // printf("Debug: Exiting signal handler\n");
}


int pthread_join(pthread_t thread, void **value_ptr){
    // should wait until the current thread finishes and is done. similar to waitpid in processes. Get the exit status from pthread_exit_wrapper();
    lock();

    if (current_thread == (int)thread) {
        printf("Error: Thread %d is trying to join itself\n", current_thread);
        unlock();
        return -1;
    }

    if (joined_threads[(int)thread]) {
        printf("Error: Thread %d is already joined\n", (int)thread);
        unlock();
        return -1;
    }

    joined_threads[(int)thread] = current_thread;
    threads_joined[current_thread] += 1;
    // printf("threads_joined[%d] = %d\n", current_thread, threads_joined[current_thread]);

    while (strcmp(threads[(int)thread].status, "EXITED") != 0){
        strcpy(threads[current_thread].status, "BLOCKED");
        printf("Thread %d is blocked\n", current_thread);
        unlock();
        schedule();
        lock();
    }

    if (value_ptr != NULL){
        *value_ptr = exit_status[(int)thread];
    }

    free(threads[(int)thread].stack);
    threads[(int)thread].stack = NULL;

    unlock();
    return 0;
}

typedef struct{
    int value;
    myQ *queue;
    int initialized;
} mySem;

mySem semaphores[MAX_THREADS];
int sem_count = 0;

union sem_t_union {
    sem_t sem;
    int __align;  
};

int sem_init(sem_t *sem, int pshared, unsigned value){
    
    lock();
    printf("Initializing semaphore\n");
    if (value < 0){
        unlock();
        return -1;
    }
    if (sem_count >= MAX_THREADS){
        unlock();
        return -1;
    }
    if (pshared != 0){
        unlock();
        return -1;
    }
    int sem_index = sem_count;
    ((union sem_t_union *)sem)->__align = sem_index;


    semaphores[sem_count].value = value;
    semaphores[sem_count].initialized = 1;
    semaphores[sem_count].queue = (myQ*)malloc(sizeof(myQ));
    init_queue(semaphores[sem_count].queue);
    sem_count++;
    printf("Semaphore %d initialized with value %d\n", sem_index, value);


    unlock();
    return 0;
}

int sem_wait(sem_t *sem){
    lock();
    printf("Waiting\n");

    int sem_index = ((union sem_t_union *)sem)->__align;

    if (semaphores[sem_index].initialized == 0){
        unlock();
        return -1;
    }
    mySem* semaphore = &semaphores[sem_index];
    printf("sem_wait: Thread %d is attempting to wait on semaphore %d, current value = %d\n", 
           current_thread, sem_index, semaphore->value);
    if (semaphore->value > 0){
        semaphore->value--;
        printf("sem_wait: Thread %d decremented semaphore %d to %d\n", 
               current_thread, sem_index, semaphore->value);
    }
    else{
        strcpy(threads[current_thread].status, "BLOCKED");
        printf("Thread %d is blocked\n", current_thread);
        printf("sem_wait: Thread %d is blocked on semaphore %d\n", 
               current_thread, sem_index);

        myTCB* thread_copy = malloc(sizeof(myTCB));
        
        memcpy(thread_copy, &threads[current_thread], sizeof(myTCB));
        enqueue(semaphore->queue, thread_copy);
        thread_yielded = 1;  

        // unlock();  
        // schedule();
        // lock(); 
    }
    unlock();
    return 0;
}

int sem_post(sem_t *sem){
    lock();
    printf("Posting\n");

    int sem_index = ((union sem_t_union *)sem)->__align;
    if (semaphores[sem_index].initialized == 0){
        unlock();
        return -1;
    }
    mySem* semaphore = &semaphores[sem_index];
 
    // printf("Thread %d is waiting on semaphore\n", semaphore->queue->front->thread->thread_id);
    semaphore->value++;
    if (semaphore->value > 0){
        myTCB* thread = dequeue(semaphore->queue);
        if (thread != NULL){
            strcpy(thread->status, "READY");
            enqueue(&ready_queue, thread);
        }
    }
    unlock();
    return 0;
}

int sem_destroy(sem_t *sem){
    lock();
    printf("Destroying semaphore\n");
    int sem_index = ((union sem_t_union *)sem)->__align;

    if (semaphores[sem_index].initialized == 0){
        unlock();
        return -1;
    }
    mySem* semaphore = &semaphores[sem_index];

    if (semaphore->queue->front != NULL){
        unlock();
        return -1;
    }
    semaphore->initialized = 0;
    free(semaphore->queue);
    semaphore->value = 0;
    semaphore->queue = NULL;
    unlock();
    return 0;
    
}

void TCB_init(){
    first_time = 0;
    init_queue(&ready_queue);
    myTCB* main_thread = (myTCB*)malloc(sizeof(myTCB));
    strcpy(main_thread->status, "RUNNING");

    main_thread->thread_id = thread_count;
    main_thread->stack = NULL;
    setjmp(main_thread->context);
    threads[0] = *main_thread;
    thread_count++;

    struct sigaction act;
    act.sa_flags = SA_NODEFER;
    act.sa_handler = signal_handler;
    sigaction(SIGALRM, &act, 0);
    ualarm(50000, 50000);
    // add schedule instantiation
    // add signal handler
    // add main file
}



int pthread_create(pthread_t *thread,
const pthread_attr_t *attr,
void *(*start_routine) (void *),
void *arg
){
    if (first_time){
        TCB_init();
    }
    
    myTCB* new_thread = (myTCB*)malloc(sizeof(myTCB));
    if (new_thread == NULL) {
        perror("Failed to allocate TCB");
        return -1;
    }
    new_thread->thread_id = thread_count;
    size_t size = 32767;
    new_thread->stack = malloc(size);
    if (new_thread->stack == NULL) {
        perror("malloc");
        free(new_thread);
        return -1;
    }
    
    
    strcpy(new_thread->status, "READY");

    // printf("Debug: Creating thread %d\n", new_thread->thread_id);
    // printf("Debug: Stack address: %p, size: %zu\n", new_thread->stack, size);
    // printf("Debug: Start routine address: %p\n", (void*)start_routine);
    // printf("Debug: Arg address: %p\n", arg);


    if (setjmp(new_thread->context) == 0) {
        unsigned long *sp = (unsigned long *)((char *)new_thread->stack + size - sizeof(unsigned long));
        *sp = (unsigned long)pthread_exit_wrapper; 
        new_thread->context[0].__jmpbuf[JB_R12] = (long unsigned int)start_routine;
        new_thread->context[0].__jmpbuf[JB_R13] = (long unsigned int)arg;
        new_thread->context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)sp);                           
        new_thread->context[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);  

        // printf("pthread_create: Context set up. JB_PC = %lu, JB_RSP = %lu\n", 
        //        ptr_demangle(new_thread->context[0].__jmpbuf[JB_PC]),
        //        ptr_demangle(new_thread->context[0].__jmpbuf[JB_RSP]));   
    }
    threads[thread_count] = *new_thread;
    *thread = thread_count;  
    thread_count++;
    enqueue(&ready_queue, new_thread);
    // return start_routine(arg);

    return 0;


    // set all registers: 4
}


void pthread_exit(void *value_ptr){
    // free
    // note to myself: check if the all threads have exited: if not schedule
    // note to myself: this function cant return, so need to keep calling schedule if we come back in
    // printf("Exiting thread %d\n", current_thread);
    lock();
    exit_status[current_thread] = value_ptr;
    strcpy(threads[current_thread].status, "EXITED");

    // printf("Thread %d exited\n", current_thread);
    // printf("joined_threads[%d] = %d\n", current_thread, joined_threads[current_thread]);

    if (joined_threads[current_thread] != -1) {
        int waiting_thread = joined_threads[current_thread];
        threads_joined[waiting_thread] -= 1;
        // printf("Thread %d is waiting for thread %d\n", waiting_thread, current_thread);
        if (threads_joined[waiting_thread] == 0 && strcmp(threads[waiting_thread].status, "BLOCKED") == 0) {
            // printf("Unblocking thread %d\n", waiting_thread);
            strcpy(threads[waiting_thread].status, "READY");
            enqueue(&ready_queue, &threads[waiting_thread]);
        }
    }

    // printf("Status of thread %d: %s\n", current_thread, threads[current_thread].status);
    // free(threads[current_thread].stack);
    // threads[current_thread].stack = NULL;
    // if (all_threads_done) {
    //     // printf("All threads have exited. Terminating program.\n");
    //     exit(0);  
    // }
    // if(all_threads_done){
    //     exit(0);
    // }
    free(threads[current_thread].stack);
    threads[current_thread].stack = NULL;
    unlock();

    schedule();
    
    
    exit(0);
}

pthread_t pthread_self(void){
    return threads[current_thread].thread_id;
}

void schedule(){
    // printf("Scheduling\n");
    lock();
    int prev_thread = current_thread;
    // printf("Current thread id: %d\n", current_thread);
    // sigset_t mask, prev_mask;
    // sigfillset(&mask);
    // sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    if (setjmp(threads[current_thread].context) == 0) {
        printf("Debug: In setjmp\n");

        myTCB* next_thread = dequeue(&ready_queue);
        while (next_thread && strcmp(next_thread->status, "BLOCKED") == 0) {
            enqueue(&ready_queue, next_thread);
            next_thread = dequeue(&ready_queue);
        }
        printf("Next thread id: %d\n", next_thread->thread_id);
        if (next_thread == NULL){
            all_threads_done = 1;
            unlock();
            return;
        }
        
        current_thread = next_thread -> thread_id;
        printf("Current thread id: %d\n", current_thread);

        if (!thread_yielded && strcmp(threads[prev_thread].status, "RUNNING") == 0) {
            strcpy(threads[prev_thread].status, "READY");
            enqueue(&ready_queue, &threads[prev_thread]);
        }
        // printf("Switching from %d to %d\n", prev_thread, current_thread);
        strcpy(threads[current_thread].status, "RUNNING");
        thread_yielded = 0;


        printf("Switching to thread %d\n", current_thread);

        // printf("Debug: About to longjmp to thread %d. JB_PC: %lu, JB_RSP: %lu\n", 
            //    current_thread, 
            //    ptr_demangle(threads[current_thread].context[0].__jmpbuf[JB_PC]),
            //    ptr_demangle(threads[current_thread].context[0].__jmpbuf[JB_RSP]));
        // sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        unlock();
        longjmp(threads[current_thread].context, 1);
        
        // printf("Done %d\n", current_thread);
    }
    printf("Scheduling done\n");
}
