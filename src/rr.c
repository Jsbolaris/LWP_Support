#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <string.h>
#include <stdbool.h>
#include "fp.h"

extern scheduler RoundRobin;
struct scheduler rr_publish = {rr_init, rr_shutdown, rr_admit, rr_remove, rr_next, rr_qlen};
scheduler RoundRobin = &rr_publish;


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
           //printf("RR Next: Switching to Thread %ld.\n", next_thread->tid);
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
