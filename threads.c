#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include "ec440threads.h"


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
    printf("Debug: Signal handler called. Signal: %d\n", sig);
    
    // sigset_t mask, prev_mask;
    // sigfillset(&mask);
    // sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    if (ready_queue.front && !all_threads_done)
    {schedule();}
    // sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    
    printf("Debug: Exiting signal handler\n");
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
    act.sa_flags = SA_NODEFER | SA_RESTART;
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
        sp--;
        *sp = (unsigned long)pthread_exit; 
        new_thread->context[0].__jmpbuf[JB_R12] = (long unsigned int)start_routine;
        new_thread->context[0].__jmpbuf[JB_R13] = (long unsigned int)arg;
        new_thread->context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)sp);                           
        new_thread->context[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);  

        // printf("pthread_create: Context set up. JB_PC = %lu, JB_RSP = %lu\n", 
        //        ptr_demangle(new_thread->context[0].__jmpbuf[JB_PC]),
        //        ptr_demangle(new_thread->context[0].__jmpbuf[JB_RSP]));   
    }
    threads[thread_count] = *new_thread;
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
    strcpy(threads[current_thread].status, "EXITED");
    // printf("Status of thread %d: %s\n", current_thread, threads[current_thread].status);
    // free(threads[current_thread].stack);
    // threads[current_thread].stack = NULL;

    if (all_threads_done) {
        // printf("All threads have exited. Terminating program.\n");
        exit(0);  
    }
    // if(all_threads_done){
    //     exit(0);
    // }
    schedule();
    free(threads[current_thread].stack);
    threads[current_thread].stack = NULL;
    exit(0);
}

pthread_t pthread_self(void){
    return threads[current_thread].thread_id;
}

void schedule(){
    // printf("Scheduling\n");
    int prev_thread = current_thread;
    // printf("Current thread id: %d\n", current_thread);
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    if (setjmp(threads[current_thread].context) == 0) {
        // printf("Debug: In setjmp\n");
        myTCB* next_thread = dequeue(&ready_queue);
        // printf("Next thread id: %d\n", next_thread->thread_id);
        if (next_thread == NULL){
            all_threads_done = 1;
            exit(0);
        }
        current_thread = next_thread -> thread_id;
        // printf("Current thread id: %d\n", current_thread);

        if (strcmp(threads[prev_thread].status, "RUNNING") == 0) {
            strcpy(threads[prev_thread].status, "READY");
            enqueue(&ready_queue, &threads[prev_thread]);
        }
        // printf("Switching from %d to %d\n", prev_thread, current_thread);
        strcpy(threads[current_thread].status, "RUNNING");

        // printf("Switching to thread %d\n", current_thread);

        // printf("Debug: About to longjmp to thread %d. JB_PC: %lu, JB_RSP: %lu\n", 
            //    current_thread, 
            //    ptr_demangle(threads[current_thread].context[0].__jmpbuf[JB_PC]),
            //    ptr_demangle(threads[current_thread].context[0].__jmpbuf[JB_RSP]));
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        longjmp(threads[current_thread].context, 1);
        
        // printf("Done %d\n", current_thread);
    }
    // printf("Scheduling done\n");
}



