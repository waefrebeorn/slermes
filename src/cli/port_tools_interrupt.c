/*
 * port_tools_interrupt.c — C port of tools/interrupt.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* PoP: cli_tools_interrupt_is_set @ tools/interrupt.py:is_set */

/*
 * Per-thread interrupt state.
 * Maps Python's threading.current_thread().ident to a boolean.
 */
#define MAX_INTERRUPT_THREADS 64

static struct {
    unsigned long tid;
    int interrupted;
} interrupt_table[MAX_INTERRUPT_THREADS];

static pthread_mutex_t interrupt_lock = PTHREAD_MUTEX_INITIALIZER;
static int interrupt_initialized = 0;

static void init_interrupt_table(void) {
    if (!interrupt_initialized) {
        memset(interrupt_table, 0, sizeof(interrupt_table));
        interrupt_initialized = 1;
    }
}

/*
 * is_set: Check if the current thread has been interrupted.
 *
 * Returns: (void*)1 if interrupted, (void*)0 otherwise.
 */
void* cli_tools_interrupt_is_set(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    init_interrupt_table();

    /* Get current thread ID — platform-specific */
    unsigned long tid = (unsigned long)pthread_self();

    pthread_mutex_lock(&interrupt_lock);

    int result = 0;
    for (int i = 0; i < MAX_INTERRUPT_THREADS; i++) {
        if (interrupt_table[i].tid == tid && interrupt_table[i].interrupted) {
            result = 1;
            break;
        }
    }

    pthread_mutex_unlock(&interrupt_lock);

    if (result) {
        hermes_log(LOG_DEBUG, "port",
                   "interrupt: thread %lu is interrupted", tid);
    }

    return (void *)(intptr_t)result;
}
