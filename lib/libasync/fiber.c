/*
 * fiber.c — Thin scheduler front-end for the Slermes async runtime.
 *
 * This tree already has a proven poll-based event loop (lib/libasync_poll,
 * used by websocket_async.c) that is callback-driven and needs no ucontext.
 * We deliberately avoid ucontext/swapcontext here: on this glibc, swapcontext
 * trips an SSE-alignment SIGSEGV and a hand-rolled stack switch is fragile.
 *
 * async_runtime_run(top, arg) therefore runs the top coroutine entry (which
 * registers async work via the event loop), then pumps the loop until it is
 * idle — the faithful C equivalent of `asyncio.run(coro())` for the
 * callback-driven IO model this runtime uses.
 */

#define _GNU_SOURCE
#include "async_runtime.h"
#include "async_poll.h"
#include <stdlib.h>
#include <string.h>

void async_runtime_run(fiber_entry_t top, void *arg) {
    async_poll_t *loop = async_poll_create(256);
    if (!loop) return;
    if (top) top(arg);
    /* Pump until the loop is idle (no fd monitors and no timers pending). */
    while (async_poll_fd_count(loop) > 0 || async_poll_timer_count(loop) > 0) {
        int n = async_poll_run_once(loop, 100);
        (void)n;
    }
    async_poll_destroy(loop);
}
