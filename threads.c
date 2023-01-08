//included libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //for ualarm
#include <setjmp.h> //for jmp functionality
#include <pthread.h> //for pthread_t type
#include <signal.h> //for SIGALARM
#include <errno.h> //for debugging

#include "threads.h"

//constants set in the assignment parameters
#define SWITCH 50000 //microseconds -> 50 ms
#define STACKSIZE 32767
#define MAXTHREADS 128

//jmp_buf indexes needed to modify jmp_buf directly. From x86_64/jmpbuf-offsets.h
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC  7

//defining thread status
#define EMPTY 0 //empty space with no thread context
#define READY 1 //thread can be scheduled
#define RUNNING 2 //thread is actively running
#define EXITED 3 //thread is done running but context has not been cleaned

//tcb structure
struct threadControlBlock{
    pthread_t threadID; //holds the thread id
    jmp_buf environment; //holds context of thread registers
    int* stack; //pointer to the bottom of the stack
    int status; //status of thread
};

//global variables
/*static means the varaibles are only accessible within the library's scope*/
static pthread_t currentThread = 0; //holds the currently running thread
static struct threadControlBlock tcbArray[MAXTHREADS]; //holds the context for all possible threads
static struct sigaction signalHandle; //new signal setup for SIG

/*--- --- --- FUNCTIONS BEGIN BELOW THIS LINE --- --- ---*/

//function to create a new thread
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg){
    printf("---------In pthread_create.---------\n");
    static int runBool = 0; //this sets when pthread is first called

    if (runBool == 0){ //if this is the first time calling pthread_create, set up
        printf("This is the first time pthread was called.\n");

        //set up tcb data structure
        for (int i = 0; i < MAXTHREADS; i++){ 
            tcbArray[i].threadID = i;
            tcbArray[i].status = EMPTY; //set the status of every thread to empty to startW
        }
        
        //set main thread to be index 0
        tcbArray[0].status = READY; //thread can be scheduled

        //initialized 50ms timer for SIGALRM 
        ualarm((useconds_t) SWITCH, (useconds_t) SWITCH); //sends the first SIGALRM after first arg ms and repeats every second arg ms

        //initializes signal handling so the process doesn't die :)
        sigemptyset(&signalHandle.sa_mask); //empties the sa_mask field so that no signals are blocked
        signalHandle.sa_handler = &schedule; //when the signalHandle signal arrives, run schedule
        signalHandle.sa_flags = SA_NODEFER; //the signal which triggered the handler will be blocked unless the SA_NODEFER flag is used
        sigaction(SIGALRM, &signalHandle, NULL); //when SIGALARM recieved, preforms action of sa_handle signalHandeler
        
        runBool++; //flags this if statement as run
        printf("Done with setup.\n");
    }

    //create a new thread from ID 0-127
    pthread_t testID = 0;

    while ((tcbArray[testID].status != EMPTY) && (testID < MAXTHREADS)){ //find an unused ID in the tcb
        testID++; //increment this until an empty ID is found
    }

    if (testID == MAXTHREADS){ //test if there are no empty threads in the array
        fprintf(stderr, "Error: Thread array is full, cannot make a new thread.\n");
        return -1; //return unsuccessful
    }

    printf("New thread ID found: %d.\n", (int) testID);
    setjmp(tcbArray[testID].environment); //save environment
    printf("Environment saved.\n");
    //assignments for new thread
    *thread = testID; //give found ID to the thread

    //allocate the stack 
    tcbArray[testID].stack = malloc(STACKSIZE); //allocate 32,767 bytes for the size of the stack
    printf("Stack allocated.\n");
    
    //put pthread_exit on top of the stack
    *(unsigned long *) (tcbArray[testID].stack + (STACKSIZE - 8)/4) = (unsigned long int) pthread_exit; //add pthread exit to the top of the stack
    printf("Pthread_exit address added.\n");
    //^^^ SEGFAULT ON THIS LINE IF THREADS BEING WRITTEN >= 3
    
    tcbArray[testID].environment[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)(tcbArray[testID].stack + (STACKSIZE - 8)/4)); //move stack pointer to the top of new stack
    printf("Stack pointer moved.\n");

    //set up environment for new thread --we should store the value of *arg in R13 and the address of start_func in R12
    tcbArray[testID].environment[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); //update the program counter to start_routine pointer
    tcbArray[testID].environment[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine; //store start routine in R12
    tcbArray[testID].environment[0].__jmpbuf[JB_R13] = (long) arg; //store *arg in R13, moved to RDI
    printf("Program counter moved.\n");

    //update thread status to READY
    tcbArray[testID].status = READY;
    printf("New thread is READY.\n");
    printf("New thread setup completed.\n");

    schedule(); //call schedule since new thread was created
    return 0; //return successful
}

void schedule(){
    printf("--The scheduler started running now.--\n");
    pthread_t newThread;
    int threadsChecked = 0;

    switch (tcbArray[currentThread].status){ //update the status of the currently running thread
        case EMPTY:
            break;
        case READY:
            break;
        case RUNNING:
            tcbArray[currentThread].status = READY;
            break;
        case EXITED:
            break;
    }

    for (int i = currentThread + 1; i <= MAXTHREADS; i++){ //start search for next thread at num after the current thread index
        threadsChecked++;
        
        if (i >= MAXTHREADS){
            i = 0; //if i > MAXTHREADS set back to the start of indexes
        } 

        //printf("Looking for thread to schedule. Currently checking: %d. Total threads checked: %d.\n", i, threadsChecked);

        if (tcbArray[i].status == READY){ //check if the thread at the array location is ready
            newThread = tcbArray[i].threadID; //if ready, set the new thread
            tcbArray[newThread].status = RUNNING; //set the status of the thread to running
            printf("Running thread: %d.\n", (int) newThread);
            break; //leave the for loop
        }

        if (threadsChecked > MAXTHREADS-1){
            exit(0); //if no ready threads, end execution
        }
    }
    printf("For loop completed. Setting up jumps...\n");
    //^^^ next thread to schedule chosen
    int jumpSave = 0;  //0 - normal return, nonzero - longjmp return
    
    jumpSave = setjmp(tcbArray[currentThread].environment); //save the context for the current thread

    if(!jumpSave){ //if not a longjump return
        printf("This is not a longjump return for thread %d.\n", (int) currentThread);
		currentThread = newThread; //switch the currently running thread to the new thread
		longjmp(tcbArray[currentThread].environment, 1); //jump to longjump 
	}
    printf("This is a longjump return for thread %d.\n", (int) currentThread);
}

void pthread_exit(void *value_ptr){
    printf("---pthread_exit running now.---\n");
    //pthread_t exitThread;
    //need to clear env?
    
    tcbArray[currentThread].status = EXITED; //set the staus to exited
    free(tcbArray[currentThread].stack); //release the resources used by the thread
    tcbArray[currentThread].status = EMPTY; //set the staus to empty

    printf("!!exited thread %d.!!\n", (int) currentThread);
    schedule(); //schedule new thread

    exit(0);
}


pthread_t pthread_self(){ //returns the global variable gCurrent
    return currentThread;
}

/*--- --- --- PROVIDED FUNCTIONS BELOW THIS LINE --- --- ---*/

void *start_thunk(){
    asm("popq %%rbp;\n" //clean up the function prolog
    "movq %%r13, %%rdi;\n" //put arg in $rdi
    "pushq %%r12;\n" //push &start_routine
    "retq;\n" //return to &start_routine
    :
    :
    : "%rdi"
);
__builtin_unreachable();
}

unsigned long int ptr_mangle(unsigned long int p){
    unsigned long int ret;
    asm("movq %1, %%rax;\n"
    "xorq %%fs:0x30, %%rax;"
    "rolq $0x11, %%rax;"
    "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

unsigned long int ptr_demangle(unsigned long int p){
    unsigned long int ret;
    asm("movq %1, %%rax;\n"
    "rorq $0x11, %%rax;"
    "xorq %%fs:0x30, %%rax;"
    "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}
