#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

thread current_thread = NULL; // current thread pointer

int thread_count = 1;

struct scheduler rr_publish = {NULL, NULL, rr admit, rr remove, rr next, rr qlen};
scheduler RoundRobin = &rr publish;




/* static struct scheduler rr_publish = {NULL, NULL};

/**
 * @param func thread to run
 * @param arg the arguments of the function
 * @param stack_size size of the stack to be created
 * 
*/
tid_t lwp_create(lwpfun func, void *arg){
    long page_size = sysconf(_SC_PAGESIZE);
    if(!new_thread){
        perror("lwp_create");
        return (tid_t)-1; //return previous thread
    }
    //getting stack size using rlimit
    struct rlimit limit;
    getrlimit(RLIMIT_STACK, &limit);
    long stack_limit = limit.rlim_cur;

    //rounding to page size
    stack_limit = (stack_limit + page_size - 1) / page_size * page_size;
    // Allocate stack using mmap
    void* stack = mmap(NULL, stack_limit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NULL; // mmap failed
    }
    //set stack size
    new_thread->stacksize = stack_limit;
    //set stack pointer to mmap result
    new_thread->stack_pointer = stack;
    //set id
    new_thread->tid = thread_count++;
    new_thread->stack_pointer = stack;


    return new_thread->tid;
    
}

p_start(void){

    //change the stack pointer
}

void lwp_yield(void){
    thread tmp;

    tmp = current_thread;


}

