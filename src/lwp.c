#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

thread current_thread = NULL; // current thread pointer

int thread_count = 1;

struct scheduler rr_publish = {NULL, NULL, rr admit, rr remove, rr next, rr qlen};
scheduler RoundRobin = &rr publish;

static scheduler current_scheduler = &round_robin;

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
    void* stack = mmap(NULL, stack_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NULL; // mmap failed
    }
    //set stack size
    new_thread->stacksize = stack_size;
    //set stack pointer to mmap result
    new_thread->stack_pointer = stack;
    //set id
    new_thread->tid = thread_count++;
    // zero out the entire state of the new thread to start with a clean slate.
    memset(&(new_thread->state), 0, sizeof(new_thread->state)); //possibly redundant

    // calculate the top of the stack. Since the stack grows downwards, we start at the high address.
    // we cast the stack pointer to a char* first to ensure that the addition operation moves the pointer by bytes.
    unsigned long *stack_ptr = (unsigned long *)((char *)new_thread->stack + stack_size);

    // the argument for the thread function is placed on the stack first. The stack pointer is decremented
    // to make room for the argument since the stack grows towards lower memory addresses.
    *(--stack_ptr) = (unsigned long)arg;

    // next, place the return address on the stack. This is the address the thread will "return" to
    // when its main function completes. By using lwp_exit, we ensure the thread can exit cleanly.
    *(--stack_ptr) = (unsigned long)lwp_exit;

    // Finally, set the thread's stack pointer (rsp) to the current top of the stack.
    // This ensures that when the thread starts executing, it will pop off the function pointer
    // and argument correctly as if it were returning into the start of the thread function.
    new_thread->state.rsp = (unsigned long)stack_ptr; 
    return (tid_t-1)
}

void lwp_start(void){
    void lwp_start(void) {
    if (!current_scheduler) {
        lwp_set_scheduler(&round_robin);
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
    tmp = current_thread;



}

