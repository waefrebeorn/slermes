/*
 * port_interrupt.c — C port of tools/interrupt.py (select helpers)
 *
 * Port of the per-thread interrupt signaling used by tools. The Python module
 * keeps a set of interrupted thread idents and exposes is_interrupted() per
 * thread, plus a backward-compat _ThreadAwareEventProxy shim. We replicate the
 * thread-scoped set with pthread_self() keys under a mutex.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* PoP: tools_interrupt_set_interrupt @ tools/interrupt.py:set_interrupt */
static pthread_mutex_t g_interrupt_lock = PTHREAD_MUTEX_INITIALIZER;
/* Bounded set of interrupted thread ids (faithful to Python's set). */
#define MAX_INTERRUPT_THREADS 256
static unsigned long g_interrupted_threads[MAX_INTERRUPT_THREADS];
static int g_interrupted_count = 0;

void tools_interrupt_set_interrupt(int active, unsigned long thread_id)
{
    pthread_mutex_lock(&g_interrupt_lock);
    if (active) {
        int found = 0;
        for (int i = 0; i < g_interrupted_count; i++) {
            if (g_interrupted_threads[i] == thread_id) { found = 1; break; }
        }
        if (!found && g_interrupted_count < MAX_INTERRUPT_THREADS) {
            g_interrupted_threads[g_interrupted_count++] = thread_id;
        }
    } else {
        for (int i = 0; i < g_interrupted_count; i++) {
            if (g_interrupted_threads[i] == thread_id) {
                g_interrupted_threads[i] = g_interrupted_threads[--g_interrupted_count];
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_interrupt_lock);
}

/* PoP: tools_interrupt_is_interrupted @ tools/interrupt.py:is_interrupted */
int tools_interrupt_is_interrupted(unsigned long thread_id)
{
    int result = 0;
    pthread_mutex_lock(&g_interrupt_lock);
    for (int i = 0; i < g_interrupted_count; i++) {
        if (g_interrupted_threads[i] == thread_id) { result = 1; break; }
    }
    pthread_mutex_unlock(&g_interrupt_lock);
    return result;
}

/* PoP: tools_interrupt__ThreadAwareEventProxy_is_set @ tools/interrupt.py:_ThreadAwareEventProxy.is_set */
/* Drop-in proxy mapping threading.Event.is_set() to the current-thread
 * interrupt state. */
int tools_interrupt__ThreadAwareEventProxy_is_set(void)
{
    return tools_interrupt_is_interrupted((unsigned long)pthread_self());
}

/* PoP: tools_interrupt_clear @ tools/interrupt.py:clear */
void tools_interrupt_clear(void) {
    /* Python: set_interrupt(False) — clear the interrupt flag. */
    tools_interrupt_set_interrupt(0, 0);
}
