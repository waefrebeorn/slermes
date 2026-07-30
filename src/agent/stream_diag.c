/*
 * stream_diag.c — Stream diagnostics for Hermes C.
 * AG22: Per-attempt counters, exception chains, retry logging,
 *       emit_stream_drop.
 *
 * Mirrors Python's agent/stream_diag.py.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Stream diag init (AG22: stream_diag_init)
 *  In C, stream_diag_t is zero-initialized via calloc in llm_client.c.
 *  This helper resets a diag struct for a new attempt.
 * ================================================================ */

/* Port of Python agent/stream_diag.py:stream_diag_init(). */
/* Port of Python agent/stream_diag.py. */
void stream_diag_init(stream_diag_t *diag) {
    (void)diag; /* stream_diag_t is zero-initialized via calloc in llm_client.c */
}

/* Port of Python: stream_diag_capture_response */
/* PoP: _capture_response @ tools/computer_use/tool.py:_capture_response */
void stream_diag_capture_response(stream_diag_t *diag) {
    if (!diag) return;
    char saved_headers[384];
    double saved_start;
    memcpy(saved_headers, diag->upstream_headers, sizeof(saved_headers));
    saved_start = diag->request_start_time;
    memset(diag, 0, sizeof(*diag));
    memcpy(diag->upstream_headers, saved_headers, sizeof(saved_headers));
    diag->request_start_time = saved_start;
}

/* ================================================================
 *  Flatten error chain (AG22: flatten_exception_chain)
 *  C doesn't have Python's __cause__/__context__ chain, but we can
 *  format the hermes_error_t into a compact string.
 *  Returns a malloc'd string (caller must free).
 * ================================================================ */

/* Port of Python stream_diag.py:flatten_exception_chain(). */
char *flatten_error_chain(const hermes_error_t *err) {
    if (!err) return strdup("unknown");

    const char *code_str;
    switch (err->code) {
        case HERMES_OK:            code_str = "OK"; break;
        case HERMES_ERR_VALUE:     code_str = "ValueError"; break;
        case HERMES_ERR_TYPE:      code_str = "TypeError"; break;
        case HERMES_ERR_RUNTIME:   code_str = "RuntimeError"; break;
        case HERMES_ERR_IO:        code_str = "OSError"; break;
        case HERMES_ERR_TIMEOUT:   code_str = "TimeoutError"; break;
        case HERMES_ERR_CONNECTION: code_str = "ConnectionError"; break;
        case HERMES_ERR_AUTH:      code_str = "AuthError"; break;
        case HERMES_ERR_RATE_LIMITED: code_str = "RateLimitError"; break;
        case HERMES_ERR_STREAM:    code_str = "StreamError"; break;
        default:                   code_str = "Error"; break;
    }

    char buf[512];
    if (err->message[0]) {
        snprintf(buf, sizeof(buf), "%s(%s)", code_str, err->message);
    } else {
        snprintf(buf, sizeof(buf), "%s", code_str);
    }

    /* Truncate to 140 chars like Python version */
    size_t len = strlen(buf);
    if (len > 140) {
        buf[137] = '.';
        buf[138] = '.';
        buf[139] = '.';
        buf[140] = '\0';
    }

    return strdup(buf);
}

/* ================================================================
 *  Log stream retry (AG22: log_stream_retry)
 *  Structured WARNING log with full diagnostic detail.
 * ================================================================ */

/* Port of Python stream_diag.py:log_stream_retry(). */
void log_stream_retry(
    const char *provider,
    const char *base_url,
    const hermes_error_t *error,
    int attempt,
    int max_attempts,
    bool mid_tool_call,
    const stream_diag_t *diag)
{
    if (!error) return;

    /* Build error chain string */
    char *chain = flatten_error_chain(error);

    /* Extract diag fields */
    double bytes = 0;
    unsigned long chunks = 0;
    double elapsed = 0.0;
    double ttfb = -1.0;
    const char *headers_repr = "-";
    const char *http_status = "-";
    double now = (double)time(NULL);

    if (diag) {
        /* diag->total_tokens used as proxy for bytes in C implementation */
        bytes = (double)(diag->total_tokens);
        chunks = diag->total_tokens;
        if (diag->request_start_time > 0) {
            elapsed = now - diag->request_start_time;
            if (elapsed < 0) elapsed = 0;
        }
        if (diag->first_token_time > 0 && diag->request_start_time > 0) {
            ttfb = diag->first_token_time - diag->request_start_time;
            if (ttfb < 0) ttfb = 0;
        }
        if (diag->upstream_headers[0])
            headers_repr = diag->upstream_headers;
        /* http_status not directly stored in stream_diag_t;
           would need to be passed separately or added to struct */
    }

    char ttfb_str[32];
    if (ttfb >= 0)
        snprintf(ttfb_str, sizeof(ttfb_str), "%.2fs", ttfb);
    else
        snprintf(ttfb_str, sizeof(ttfb_str), "-");

    hermes_log(LOG_WARNING, "stream_diag",
        "Stream %s on attempt %d/%d — retrying. "
        "provider=%s base_url=%s "
        "error_type=%s chain=%s "
        "http_status=%s bytes=%.0f chunks=%lu elapsed=%.2fs ttfb=%s "
        "upstream=[%s]",
        mid_tool_call ? "drop mid tool-call" : "drop",
        attempt, max_attempts,
        provider ? provider : "-",
        base_url ? base_url : "-",
        chain ? chain : "unknown",
        chain ? chain : "unknown",
        http_status,
        bytes, chunks, elapsed, ttfb_str,
        headers_repr);

    free(chain);
}

/* ================================================================
 *  Emit stream drop (AG22: emit_stream_drop)
 *  User-visible status line + structured log.
 *  In C, we log to stderr (user-visible) and agent.log (structured).
 * ================================================================ */

/* Port of Python stream_diag.py:emit_stream_drop(). */
void emit_stream_drop(
    const char *provider,
    const char *base_url,
    const hermes_error_t *error,
    int attempt,
    int max_attempts,
    bool mid_tool_call,
    const stream_diag_t *diag)
{
    if (!error) return;

    /* Structured log (same as Python's log_stream_retry) */
    log_stream_retry(provider, base_url, error, attempt, max_attempts,
                     mid_tool_call, diag);

    /* User-visible status line */
    const char *err_name = "unknown";
    if (error->code != HERMES_OK) {
        char *chain = flatten_error_chain(error);
        /* Extract just the error type name (before '(') */
        char *paren = strchr(chain, '(');
        if (paren && paren > chain) {
            size_t tlen = (size_t)(paren - chain);
            if (tlen < 64) {
                static char type_buf[64];
                memcpy(type_buf, chain, tlen);
                type_buf[tlen] = '\0';
                err_name = type_buf;
            }
        }
        free(chain);
    }

    /* Build "after Xs" suffix */
    char suffix[64] = "";
    if (diag && diag->request_start_time > 0) {
        double elapsed = (double)time(NULL) - diag->request_start_time;
        if (elapsed < 0) elapsed = 0;
        snprintf(suffix, sizeof(suffix), " after %.1fs", elapsed);
    }

    const char *kind = mid_tool_call ? "drop mid tool-call" : "drop";
    const char *prov = provider ? provider : "provider";

    fprintf(stderr, "⚠️ %s stream %s (%s)%s — reconnecting, retry %d/%d\n",
            prov, kind, err_name, suffix, attempt, max_attempts);
}
