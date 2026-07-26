/*
 * async_runtime.h — Umbrella API for the Slermes async runtime + IO runtime.
 *
 * This is the C11 faithful equivalent of Python's asyncio event loop plus
 * coroutines (async def / await). It is built on top of lib/libasync_poll
 * (a poll()-based multiplexer) and adds:
 *
 *   1. Fibers (coroutines) — cooperative ucontext-based stacks so a ported
 *      `async def` function can `await` another without blocking the loop.
 *   2. An async HTTP/HTTPS client whose socket I/O is driven non-blocking
 *      through the event loop (one thread, many concurrent in-flight
 *      requests), with TLS via OpenSSL.
 *
 * The HTTP client accepts an INJECTABLE transport so the request/response
 * parse logic is faithfully testable offline (no network), while the real
 * OpenSSL-backed transport is used in production. This mirrors the
 * port_models_net.h injectable-transport pattern.
 */

#ifndef SLERMES_ASYNC_RUNTIME_H
#define SLERMES_ASYNC_RUNTIME_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════════════════
 * 1. EVENT-LOOP RUNTIME
 * ════════════════════════════════════════════════════════════════════════ */

/* Entry point for a top-level async task (registers work on the loop). */
typedef void (*fiber_entry_t)(void *arg);

/* The high-level entry point: run a top-level coroutine entry (which registers
 * async work on the event loop) and pump the loop until it is idle. This is
 * the C11 faithful equivalent of `asyncio.run(coro())` for the callback-driven
 * IO model this runtime uses (mirroring websocket_async.c — no ucontext).
 * NOTE: this runtime uses a poll-based event loop (lib/libasync_poll) and a
 * callback-driven async HTTP client; "await" maps to registering a completion
 * callback and pumping the loop, not to stack-switching coroutines. */
void async_runtime_run(fiber_entry_t top, void *arg);

/* ════════════════════════════════════════════════════════════════════════
 * 2. ASYNC HTTP CLIENT
 * ════════════════════════════════════════════════════════════════════════ */

typedef struct async_http_result_t {
    int    code;        /* HTTP status (0 on transport error) */
    int    transport_err;
    char  *body;        /* malloc'd response body (may be NULL) */
    size_t len;
    char  *err;         /* malloc'd error string when code==0 */
} async_http_result_t;

void async_http_result_free(async_http_result_t *r);

typedef struct async_http_client_t async_http_client_t;

async_http_client_t *async_http_client_new(int timeout_ms);
void async_http_client_free(async_http_client_t *c);

/* Injectable transport — used for offline oracle tests. The transport
 * performs the actual request and fills *out (it owns out->body/out->err).
 * Returns 0 on success, non-zero on error. Mirrors port_models_net.h. */
typedef int (*async_http_transport_t)(const char *method, const char *url,
                                       const char *headers, const char *body,
                                       async_http_result_t *out, void *ctx);

void async_http_set_transport(async_http_client_t *c,
                              async_http_transport_t t, void *ctx);

/* These are AWAITABLE from within a fiber (they call fiber_yield while the
 * socket is not ready). When called outside a fiber they still work but run
 * synchronously on an internal loop. They return a malloc'd result. */
async_http_result_t *async_http_get(async_http_client_t *c, const char *url);
async_http_result_t *async_http_post_json(async_http_client_t *c,
                                           const char *url, const char *json_body);

/* Parse a single HTTP response (status line + headers + body) out of `buf`
 * (len bytes). Used by the real transport and exercised directly by tests.
 * Fills *out (caller must async_http_result_free). Returns 0 on parse ok. */
int async_http_parse_response(const char *buf, size_t len,
                              async_http_result_t *out);

/* Parse a URL into its components (malloc'd; caller frees with
 * async_http_url_free). Returns 0 on success. */
typedef struct {
    char *scheme;   /* "http" | "https" */
    char *host;
    int   port;     /* default 80/443 if unspecified */
    char *path;     /* includes query string, always begins with '/' */
} async_http_url_t;
int  async_http_parse_url(const char *url, async_http_url_t *out);
void async_http_url_free(async_http_url_t *u);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_ASYNC_RUNTIME_H */
