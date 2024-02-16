#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <string.h>


// Default Round-Robin Scheduler Functions
static thread rr_list_head = NULL;

// current thread pointer
thread current_thread = NULL; 




void log_linked_list(void) {
    if (rr_list_head == NULL) {
        return;
    }
    thread temp = rr_list_head;
    do {
        temp = temp->lib_one;
    } while (temp != rr_list_head);
}


static void rr_init(void) {
    rr_list_head = NULL; // Initialize the list head
}

static void rr_shutdown(void) {
    // Clean up the thread list if necessary
    thread current = rr_list_head;
    while (current != NULL) {
        thread next = current->lib_one;
        free(current);
        current = next;
    }
    rr_list_head = NULL;
}


static void rr_admit(thread new) {
    new->lib_one = NULL;  // Initially, no next thread.
    if (rr_list_head == NULL) {
        rr_list_head = new;
        new->lib_one = new;  // Points to itself, forming a circular list.
        current_thread = new;  // Set the current thread to the new thread if it's the first.
    } else {
        thread last = rr_list_head;
        while (last->lib_one != rr_list_head) {  // Traverse to find the last thread.
            last = last->lib_one;
        }
        last->lib_one = new;  // Link the new thread to the last.
        new->lib_one = rr_list_head;  // Complete the circle by linking back to the head.
    }
    log_linked_list();  // To check the list after admission.
}



static void rr_remove(thread victim) {
    // Remove the victim from the list
    if (rr_list_head == victim) {
        rr_list_head = victim->lib_one;
    } else {
        thread current = rr_list_head;
        while (current != NULL && current->lib_one != victim) {
            current = current->lib_one;
        }
        if (current != NULL) {
            current->lib_one = victim->lib_one;
        }
    }
    log_linked_list();
}

static thread rr_next(void) {
    if (current_thread == NULL || current_thread->lib_one == current_thread) {
        // Case where only one thread exists or no current thread is set
        return NULL;
    } else {
        thread next_thread = current_thread->lib_one;
        if (next_thread == rr_list_head) {
        } else {
           printf("RR Next: Switching to Thread %ld.\n", next_thread->tid);
        }
        return next_thread;
    }
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

/* static struct scheduler rr_publish = {NULL, NULL};

/**
 * @param func thread to run
 * @param arg the arguments of the function
 * 
*/
tid_t lwp_create(lwpfun func, void *arg){
    long page_size = sysconf(_SC_PAGESIZE);
    thread new_thread = malloc(sizeof(context));
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
        return NULL; // mmap failed
    }
    //set stack size
    new_thread->stacksize = stack_size;
    //set stack pointer to mmap result
    new_thread->stack = stack;
    //set id
    new_thread->tid = thread_count++;

    // calculate the top of the stack. Since the stack grows downwards, we start at the high address.
    unsigned long *stack_ptr = (unsigned long *)(stack + stack_size);
    //we just place lwp_wrap on the stack
    *(--stack_ptr) = (unsigned long)lwp_wrap;
    
    new_thread->state.rsp = (unsigned long)stack_ptr;
    current_scheduler->admit(new_thread);
    return new_thread->tid;
}

void lwp_start(void){
    void lwp_start(void) {
    if (!current_scheduler) {
        lwp_set_scheduler(&rr_publish);
    }
    current_scheduler->init();
    
    // Allocate memory for the initial thread context but do not admit it to the scheduler.
    thread initial_thread = malloc(sizeof(context));
    if (!initial_thread) {
        perror("LWP Start: Failed to allocate initial thread");
        return;
    }
    initial_thread->tid = 0;  // Optionally assign a special ID or handle for the initial thread.
    initial_thread->stack = NULL;  // Initial thread uses the main stack.
    initial_thread->state.rsp = 0;  // Reset the stack pointer for the initial thread.
    // Do not admit the initial thread to the round-robin scheduler.

    // Directly yield to user-created threads.
    lwp_yield();
    }
}

void lwp_yield(void){


    thread prev_thread = current_thread;
    //save context
    swap_rfiles(&(prev_thread->state), &(current_thread->state));

}


void lwp_exit(int status){
    current_scheduler->remove(current_thread);

}

static void lwp_wrap(lwpfun fun, void *arg){
    int rval;
    rval=fun(arg);
    lwp_exit(rval);
}


//test function
void thread_func(void *arg){
    printf("Thread function is running with argument %p\n", arg);
}


int main(){
    if (current_scheduler->init){
        current_scheduler->init();
    }
    tid_t new_tid = lwp_create(thread_func,(void*)0x1234);
    if (new_tid == (tid_t)-1) {
        perror("Failed to create thread");
        return 1;
    } else {
        printf("Thread created with TID: %ld\n", new_tid);
    }
}

