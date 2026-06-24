/*
 * port_tools_thread_context.c — C port of tools/thread_context.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_thread_context__callback_api @ tools/thread_context.py:_callback_api */

/* Port of Python tools/thread_context.py:_callback_api */
/* Resolve the terminal_tool callback getters/setters. */
/* In C, callbacks are managed via function pointers. */
/* Returns the current approval callback handle, or NULL if unavailable. */
void *cli_tools_thread_context__callback_api(int which)
{
    /* which: 0=get_approval, 1=get_sudo, 2=set_approval, 3=set_sudo */
    /* In C, these are handled via the approval module's internal state.
     * For now, return NULL as the threading model differs from Python. */
    (void)which;
    hermes_log(LOG_DEBUG, "thread_context", "callback_api: not available in C");
    return NULL;
}

/* PoP: cli_tools_thread_context_propagate_context_to_thread @ tools/thread_context.py:propagate_context_to_thread */

/* Port of Python tools/thread_context.py:propagate_context_to_thread */
/* Wrap target for execution on a worker thread with the current thread's
 * ContextVars and approval/sudo callbacks propagated.
 *
 * In C, threading uses pthreads and context propagation is handled
 * by passing explicit context pointers. This function returns a
 * wrapper context that callers can use with pthread_create. */
typedef struct {
    void *(*target)(void *);
    void *target_arg;
    /* Propagated context would go here */
} thread_context_t;

thread_context_t *cli_tools_thread_context_propagate_context_to_thread(
    void *(*target)(void *), void *arg)
{
    if (!target) return NULL;

    thread_context_t *ctx = (thread_context_t *)malloc(sizeof(thread_context_t));
    if (!ctx) return NULL;

    ctx->target = target;
    ctx->target_arg = arg;

    /* In a full implementation, this would also capture:
     * - Current approval callback
     * - Current sudo password callback
     * - ContextVars (equivalent)
     * These would be restored in the thread runner function. */

    return ctx;
}
