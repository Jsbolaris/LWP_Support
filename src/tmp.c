tid_t lwp_wait(int *status){
    int tid = NO_THREAD;
    thread dead_thread;

    //check for threads in terminated queue
    if(!isEmpty(terminated_threads)){
        //dequeue oldest terminated thread
        dead_thread = dequeue(terminated_threads);
    }
    else{
        //check for running threads
        if(Scheduler->qlen() <= 1){
            //return NO_THREAD
            return tid;
        }
        //BLOCK
        //remove current thread from scheduler
        Scheduler->remove(current_thread);
        //add current thread to waiting queue
        enqueue(waiting_threads, current_thread);
        //by yielding when current thread isnt scheduled
        //we gaurentee this thread will not run again 
        //until an exited thread reschedules it
        lwp_yield();
        //if we're here, exited must be non NULL
        dead_thread = current_thread->exited;
    }

    //Save dead thread tid for return value
    tid = dead_thread->tid;

    //Remove dead thread from scheduler
    Scheduler->remove(dead_thread);

    //populate status
    if(status){
        *status = dead_thread->status;
    }

    //deallocate rfile


    //deallocate stack
    dead_thread->stack = dead_thread->stack - 
                        (dead_thread->stacksize/sizeof(unsigned long));
    munmap(dead_thread->stack, dead_thread->stacksize);

    //free thread
    free(dead_thread);
    return tid;
}