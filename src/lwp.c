#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <string.h>
#include <stdbool.h>
#include "fp.h"

// Debug Logging Utility
#define DEBUG_PRINT(...) \
    do { printf(__VA_ARGS__); fflush(stdout); } while (0)

// Default Round-Robin Scheduler Functions

static thread thread_list = NULL;
// current thread pointer
thread current_thread = NULL; 
static thread rr_list_head = NULL;
void log_linked_list(void) {
    if (rr_list_head == NULL) {

        return;
    }

    thread temp = rr_list_head;
    do {
        temp = temp->lib_one;
    } while (temp != rr_list_head);
}

static void rr_admit(thread new) {
    if (rr_list_head == NULL) {
        rr_list_head = new;
        new->lib_one = new; // Points to itself, forming a circular list
    } else {
        // Insert the new thread before the head to maintain the list as circular without traversing it
        thread tail = rr_list_head->lib_one;
        rr_list_head->lib_one = new;
        new->lib_one = tail;
    }
    log_linked_list();
}

static void rr_remove(thread victim) {
    if (rr_list_head == NULL || victim == NULL) {
        return;
    }

    // Special case: single-element list
    if (victim == rr_list_head && victim->lib_one == victim) {
        rr_list_head = NULL;
    } else {
        thread prev = rr_list_head;
        while (prev->lib_one != victim && prev->lib_one != rr_list_head) {
            prev = prev->lib_one;
        }

        if (prev->lib_one == victim) {
            prev->lib_one = victim->lib_one;
            // Adjust rr_list_head if necessary
            if (rr_list_head == victim) {
                rr_list_head = victim->lib_one;
            }
        } else {
            return;
        }
    }
    victim->lib_one = NULL; // Clear to help with debugging and prevent accidental reuse
    log_linked_list();
}


static thread rr_next(void) {
    // Check if the list is empty or contains only one thread, no switch needed
    if (rr_list_head == NULL || rr_list_head->lib_one == rr_list_head) {
        return NULL;
    }

    // Ensure current_thread is valid and part of the round-robin list
    if (current_thread == NULL || current_thread == rr_list_head || current_thread->lib_one == NULL) {
        current_thread = rr_list_head;
    }

    thread next_thread = current_thread->lib_one;

    return next_thread;
}



static int rr_qlen(void) {
    int count = 0;
    thread current = rr_list_head;
    while (current != NULL) {
        count++;
        current = current->lib_one;
    }
    return count;
}

// Round-Robin Scheduler structure
static struct scheduler rr_publish = {
    NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen
};

static scheduler current_scheduler = &rr_publish;

int thread_count = 0;
//helper function for lwp_create
static void lwp_wrap(lwpfun fun, void *arg){
    int rval;
    rval=fun(arg);
    lwp_exit(rval);
}

/*
 * @param func thread to run
 * @param arg the arguments of the function
*/
tid_t lwp_create(lwpfun func, void *arg){
    long page_size = sysconf(_SC_PAGESIZE);
    thread new_thread = malloc(sizeof(context));
    if (!current_scheduler) {
        lwp_set_scheduler(&rr_publish);
        current_scheduler->init();
    }
    if(!new_thread){
        perror("lwp_create");
        return (tid_t)-1; //return previous thread
    }
    struct rlimit rlim;
    size_t stack_size;
    //getting stack size using rlimit
    if (getrlimit(RLIMIT_STACK, &rlim) == 0 && rlim.rlim_cur != RLIM_INFINITY) {
        stack_size = rlim.rlim_cur;
    } else {
        stack_size = 8 * 1024 * 1024; // Default to 8 MB
    }

    //rounding to page size
    stack_size = (stack_size + page_size - 1) / page_size * page_size;
    // Allocate stack using mmap
    void* stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return (tid_t) -1; // mmap failed
    }
    //set stack size
    new_thread->stacksize = stack_size;
    //set stack pointer to mmap result
    new_thread->stack = (unsigned long *)(stack + (stack_size / (sizeof(unsigned long))));
    //set id
    new_thread->tid = thread_count++;

    // calculate the top of the stack. Since the stack grows downwards, we start at the high address.
    //we just place lwp_wrap on the stack
    *(--new_thread->stack) = (unsigned long)lwp_wrap;
    *(--new_thread->stack);
    new_thread->state.rsp =  (unsigned long)new_thread->stack;
    new_thread->state.rbp =  (unsigned long)new_thread->stack;
    new_thread->state.rdi = (unsigned long)func;
    new_thread->state.rsi = (unsigned long)arg;
    new_thread->state.fxsave = FPU_INIT;
    new_thread->status = LWP_LIVE;
    current_scheduler->admit(new_thread);
    //DEBUG_PRINT("Thread %ld created and admitted with stack base: %p, rsp: %lu\n", new_thread->tid, new_thread->stack, new_thread->state.rsp);
    return new_thread->tid;
}



void lwp_start(void) {
    // Allocate memory for the initial thread context but do not admit it to the scheduler.
    thread initial_thread = malloc(sizeof(context));
    if (!initial_thread) {
        perror("LWP Start: Failed to allocate initial thread");
        return;
    }
    initial_thread->tid = 0;  // Optionally assign a special ID or handle for the initial thread.
    initial_thread->stack = NULL;  // Initial thread uses the main stack.
    initial_thread->status = LWP_LIVE;
    initial_thread->stacksize = 0;
    swap_rfiles(&initial_thread->state, NULL);
    initial_thread->state.fxsave = FPU_INIT;
    current_scheduler->admit(initial_thread);
    // Directly yield to user-created threads.
    lwp_yield();
}



tid_t lwp_wait(int *status) {
    thread prev = NULL;
    tid_t terminated_tid = NO_THREAD;
    // Block until a terminated thread is found or no more runnable threads
    while (1) {
        thread temp = thread_list;
        //iterate through list to find a terminated thread
        while (temp != NULL) {
            //thread marked for termination
            if (LWPTERMINATED(temp->status) && temp->tid != 0) {
                if (prev) {
                    prev->lib_one = temp->lib_one;
                } else {
                    thread_list = temp->lib_one; // Adjust head if first thread is removed
                }
                terminated_tid = temp->tid;
                if (status) {
                    *status = LWPTERMSTAT(temp->status);
                }
                // Safe to deallocate resources
                if (temp->stack) {
                    munmap(temp->stack, temp->stacksize);
                }
                free(temp);
                return terminated_tid; // Return the tid of the terminated thread
            }
            prev = temp;
            temp = temp->lib_one;
        }
        if (current_scheduler->qlen() <= 1) {
            // No more threads to wait for
            return NO_THREAD;
        }
        // Yield to other threads, waiting for one to terminate
        lwp_yield();
    }
}


void lwp_yield(void){

    thread prev_thread = current_thread;
    thread next_thread = current_scheduler->next();
    //save context and swap to next thread if there is another thread to switch to
    if(next_thread){
        swap_rfiles(&(prev_thread->state), &(next_thread->state));
    }else{
        exit(3);
    }

}


void lwp_exit(int status){
    current_thread->status = MKTERMSTAT(LWP_TERM, status);
    current_scheduler->remove(current_thread);


    if(current_scheduler->qlen() == 0){
        exit(EXIT_SUCCESS);
    }
    else{
        lwp_yield();
    }
}

void lwp_set_scheduler(scheduler fun) {
    current_scheduler = fun;
    current_scheduler->init();    
}

scheduler lwp_get_scheduler(void) {
    return current_scheduler;
}

thread tid2thread(tid_t tid) {
    // Search for thread by tid
    thread t = thread_list;
    while (t) {
        if (t->tid == tid) {
            return t;
        }
        t = t->lib_one;
    }
    return NULL;
}

//test function
void thread_func(void *arg){
    printf("Thread function is running with argument %p\n", arg);
}