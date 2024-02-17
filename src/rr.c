#include "lwp.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <string.h>
#include <stdbool.h>
#include "fp.h"
static thread rr_list_head = NULL;
void log_linked_list(void) {
    if (rr_list_head == NULL) {
        DEBUG_PRINT("Linked List: Empty\n");
        return;
    }

    thread temp = rr_list_head;
    DEBUG_PRINT("Linked List Start: ");
    do {
        DEBUG_PRINT("Thread %ld -> ", temp->tid);
        temp = temp->lib_one;
    } while (temp != rr_list_head);
    DEBUG_PRINT("Back to Head Thread %ld\n", rr_list_head->tid);
}


static void rr_init(void) {
    DEBUG_PRINT("RR Scheduler: Initialized\n");
    rr_list_head = NULL; // Initialize the list head
}

static void rr_shutdown(void) {
    DEBUG_PRINT("RR Scheduler: Shutdown\n");
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
    if (rr_list_head == NULL) {
        rr_list_head = new;
        new->lib_one = new; // Points to itself, forming a circular list
    } else {
        // Insert the new thread before the head to maintain the list as circular without traversing it
        thread tail = rr_list_head->lib_one;
        rr_list_head->lib_one = new;
        new->lib_one = tail;
    }
    DEBUG_PRINT("RR Scheduler: Admitted thread %ld\n", new->tid);
    log_linked_list();
}

static void rr_remove(thread victim) {
    if (rr_list_head == NULL || victim == NULL) {
        DEBUG_PRINT("RR Scheduler: Remove called with empty list or null victim.\n");
        return;
    }
    DEBUG_PRINT("RR Scheduler: Removing thread %ld\n", victim->tid);

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
            DEBUG_PRINT("RR Scheduler: Victim not found in the list.\n");
            return;
        }
    }
    victim->lib_one = NULL; // Clear to help with debugging and prevent accidental reuse
    log_linked_list();
}


static thread rr_next(void) {
    DEBUG_PRINT("Entering rr_next. rr_list_head: %p, current_thread: %ld\n", 
                (void*)rr_list_head, (current_thread ? current_thread->tid : -1));

    // Check if the list is empty or contains only one thread, no switch needed
    if (rr_list_head == NULL || rr_list_head->lib_one == rr_list_head) {
        DEBUG_PRINT("RR Next: Insufficient threads for switching. Returning NULL.\n");
        return NULL;
    }

    // Ensure current_thread is valid and part of the round-robin list
    if (current_thread == NULL || current_thread == rr_list_head || current_thread->lib_one == NULL) {
        DEBUG_PRINT("RR Next: Current thread is invalid or not part of the list, defaulting to rr_list_head.\n");
        current_thread = rr_list_head;
    }

    thread next_thread = current_thread->lib_one;
    DEBUG_PRINT("RR Next: Switching to Thread %ld.\n", next_thread->tid);

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
