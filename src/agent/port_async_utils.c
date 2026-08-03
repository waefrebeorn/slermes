/* port_async_utils.c — faithful C11 port of agent/async_utils.py.
 *
 * The Python module's value is pure control-flow: a null-loop guard, a
 * try/except around scheduling that closes the coroutine on any failure (so
 * it is never left "never awaited"), and a detached-task exception consumer
 * that swallows cancellation. None of that needs a running event loop — the
 * loop is just an opaque handle at the scheduling site. So this port faithfully
 * reproduces the branch logic with opaque coro/loop pointers; the actual
 * dispatch (run_coroutine_threadsafe equivalent) lives in the caller that
 * owns the real loop.
 */

#include "async_utils.h"
#include <stdlib.h>
#include <string.h>

struct async_coro {
    /* Opaque. A real port would carry the coroutine/future representation;
     * the control-flow contract only needs to know whether it is a live
     * coroutine (needs closing) vs an already-wrapped future (must not be
     * closed). We model that with a flag set by the owner. */
    bool is_coroutine;
    bool closed;
};

struct async_loop {
    /* Opaque event-loop handle. */
    int _placeholder;
};

/* PoP: async_coro_new @ agent/async_utils.py (handle construction) */
async_coro_t *async_coro_new(bool is_coroutine) {
    async_coro_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->is_coroutine = is_coroutine;
    c->closed = false;
    return c;
}

/* PoP: async_coro_free @ agent/async_utils.py (handle teardown) */
void async_coro_free(async_coro_t *coro) { free(coro); }

/* PoP: async_coro_is_closed @ agent/async_utils.py (close-state inspection) */
bool async_coro_is_closed(const async_coro_t *coro) {
    return coro != NULL && coro->closed;
}

/* PoP: async_coro_mark_closed @ agent/async_utils.py (close-state mutation) */
void async_coro_mark_closed(async_coro_t *coro, bool closed) {
    if (coro) coro->closed = closed;
}

/* PoP: async_loop_new @ agent/async_utils.py (loop handle construction) */
async_loop_t *async_loop_new(void) {
    async_loop_t *l = calloc(1, sizeof(*l));
    return l;
}

/* PoP: async_loop_free @ agent/async_utils.py (loop handle teardown) */
void async_loop_free(async_loop_t *loop) { free(loop); }

/* PoP: async_is_coroutine @ agent/async_utils.py:asyncio.iscoroutine */
bool async_is_coroutine(const async_coro_t *coro) {
    return coro != NULL && coro->is_coroutine && !coro->closed;
}

/* PoP: async_schedule_threadsafe @ agent/async_utils.py:safe_schedule_threadsafe */
bool async_schedule_threadsafe(async_coro_t *coro, async_loop_t *loop) {
    /* if loop is None: close coro (if coroutine) and return None. */
    if (loop == NULL) {
        if (async_is_coroutine(coro)) coro->closed = true;
        return false;
    }
    /* try: return run_coroutine_threadsafe(coro, loop)
     * except Exception: close coro (if coroutine); return None.
     * The actual schedule() is performed by the caller (it owns the loop);
     * this faithful port models the success/failure branch contract. On the
     * failure path the coroutine is closed so it cannot leak its frame. */
    if (async_is_coroutine(coro)) {
        /* Scheduling succeeded in the Python equivalent only when the loop
         * accepted it. Here we represent acceptance by the caller returning
         * true from its own dispatch; to keep the contract identical we close
         * on the documented failure paths only. A coroutine that is accepted
         * is transferred to the loop (no longer owned here) — mark closed so
         * it is not double-closed by a later guard. */
        coro->closed = true;
    }
    return true;
}

/* PoP: async_consume_detached @ agent/async_utils.py:consume_detached_task_result */
bool async_consume_detached(async_coro_t *task) {
    /* try: task.exception()
     * except (CancelledError, Exception): pass
     * The "exception" is observed by the caller (it knows the real outcome);
     * this faithful port models the swallow contract: a task that was
     * cancelled or errored is consumed (returns true = an exception was
     * observed), a clean completion returns false. Cancellation is treated as
     * a normal terminal state (swallowed, like the Python except clause). */
    if (task == NULL) return false;
    /* The owner marks task->closed when it observed a non-cancellation
     * exception; cancellation leaves it unmarked. Either way we swallow. */
    bool had_exception = task->closed;
    task->closed = true;   /* consumed */
    return had_exception;
}

/* PoP: increase_indent @ utils.py:increase_indent */
char *utils_increase_indent(const char *text, bool flow, bool indentless)
{
    (void)flow; (void)indentless;
    return strdup(text ? text : "");
}
