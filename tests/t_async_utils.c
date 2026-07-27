/*
 * t_async_utils.c — behavioral test for the faithful C11 port of
 * agent/async_utils.py (src/agent/port_async_utils.c). Self-verifying.
 */

#include "async_utils.h"
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } \
                       else printf("ok: %s\n", m); } while (0)

int main(void) {
    async_loop_t *loop = async_loop_new();

    /* --- is_coroutine guard --- */
    async_coro_t *live = async_coro_new(true);
    async_coro_t *notc = async_coro_new(false);
    CHECK(async_is_coroutine(live), "live coroutine detected");
    CHECK(!async_is_coroutine(notc), "wrapped-future not a coroutine");
    async_coro_free(live); async_coro_free(notc);

    /* --- null loop: close + false --- */
    async_coro_t *c1 = async_coro_new(true);
    CHECK(!async_schedule_threadsafe(c1, NULL), "null loop -> false");
    CHECK(async_coro_is_closed(c1), "null loop closes the coroutine (no leak)");
    async_coro_free(c1);

    /* --- real loop: success=true, coroutine transferred (closed) --- */
    async_coro_t *c2 = async_coro_new(true);
    CHECK(async_schedule_threadsafe(c2, loop), "loop present -> true");
    CHECK(async_coro_is_closed(c2), "accepted coroutine marked closed (loop owns it)");
    async_coro_free(c2);

    /* --- wrapped future on null loop: NOT closed (must not be) --- */
    async_coro_t *c3 = async_coro_new(false);
    CHECK(!async_schedule_threadsafe(c3, NULL), "null loop, future -> false");
    CHECK(!async_coro_is_closed(c3), "null loop does NOT close a wrapped future");
    async_coro_free(c3);

    /* --- consume_detached: clean completion (false) --- */
    async_coro_t *t1 = async_coro_new(true);
    async_coro_mark_closed(t1, false);            /* owner observed clean completion */
    CHECK(!async_consume_detached(t1), "clean task -> false (no exception observed)");
    CHECK(async_coro_is_closed(t1), "task consumed (closed)");
    async_coro_free(t1);

    /* --- consume_detached: errored task (true, but swallowed) --- */
    async_coro_t *t2 = async_coro_new(true);
    async_coro_mark_closed(t2, true);             /* owner observed a non-cancellation exception */
    CHECK(async_consume_detached(t2), "errored task -> true (exception observed)");
    CHECK(async_coro_is_closed(t2), "errored task consumed");
    async_coro_free(t2);

    /* --- consume_detached: null task --- */
    CHECK(!async_consume_detached(NULL), "null task -> false");

    async_loop_free(loop);
    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
