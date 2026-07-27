/* async_utils.h — async/sync bridging helpers, faithful C11 port of
 * agent/async_utils.py (leak-safe coroutine scheduling + detached-result
 * consumption). The event loop is an opaque handle here: the Python logic is
 * pure control-flow (null guard, try/except close-on-failure); no event-loop
 * execution happens in this module, so a real loop is only needed at the call
 * site that actually dispatches the coroutine. Implemented in
 * src/agent/port_async_utils.c.
 */

#ifndef SLERMES_ASYNC_UTILS_H
#define SLERMES_ASYNC_UTILS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque coroutine + event-loop handles. The C tree has no asyncio; the
 * scheduling site that actually dispatches the work owns the real loop. This
 * module only reasons about null/closed guards and cleanup on failure — the
 * faithful port of the *control flow*, not the loop. */
typedef struct async_coro async_coro_t;
typedef struct async_loop async_loop_t;

/* Handle lifecycle (a real scheduling site owns the loop and constructs
 * coroutine handles). is_coroutine mirrors Python's asyncio.iscoroutine:
 * true for a live coroutine that must be closed on failure paths, false for
 * an already-wrapped future that must NOT be closed. */
async_coro_t *async_coro_new(bool is_coroutine);
void async_coro_free(async_coro_t *coro);
/* Test/inspection accessors for the opaque close state (mirrors the Python
 * "coroutine was never awaited" lifecycle tracking). */
bool async_coro_is_closed(const async_coro_t *coro);
void async_coro_mark_closed(async_coro_t *coro, bool closed);
async_loop_t *async_loop_new(void);
void async_loop_free(async_loop_t *loop);

/* Mirrors Python's asyncio.iscoroutine(): whether the handle is a live
 * coroutine that needs closing on the failure paths. A non-coroutine
 * (e.g. an already-wrapped future) must NOT be closed. */
bool async_is_coroutine(const async_coro_t *coro);

/* Faithful port of safe_schedule_threadsafe: schedule coro on loop from a
 * sync context, leak-safe. Returns true on success (caller owns the future
 * lifecycle), false when the loop is NULL or scheduling raised (in which case
 * the coroutine is closed so it does not leak its frame / emit
 * "was never awaited"). The loop is an opaque pointer; the real dispatch
 * happens in the caller. */
bool async_schedule_threadsafe(async_coro_t *coro, async_loop_t *loop);

/* Faithful port of consume_detached_task_result: observe a detached task's
 * exception without surfacing cancellation. Swallows CancelledError and any
 * terminal error — the task's owner already gave up on it. Returns true when
 * an exception (other than cancellation) was observed, false on clean
 * completion / cancellation. The "result" is an opaque pointer the caller
 * provides (e.g. a captured error string); this module only drives the
 * observe-and-swallow contract. */
bool async_consume_detached(async_coro_t *task);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_ASYNC_UTILS_H */
