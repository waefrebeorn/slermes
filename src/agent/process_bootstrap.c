/*
 * process_bootstrap.c — Process-level bootstrap helpers (port of
 * Python agent/process_bootstrap.py).
 *
 * Three concerns, matching the Python module:
 *   1. Lazy OpenAI SDK proxy       (_OpenAIProxy — C has no Python SDK)
 *   2. Crash-resistant stdio       (_SafeWriter — swallows broken-pipe errors)
 *   3. HTTP proxy / keepalive client (build_keepalive_http_client)
 *
 * _get_proxy_from_env / _get_proxy_for_base_url live in proxy_utils.c.
 * The process-runner helpers (run_with_timeout, etc.) live in async_utils.c.
 *
 * Self-contained: includes only core types + the proxy/http it needs.
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "process_bootstrap.h"
#include "hermes_proxy_utils.h"
#include "hermes_http.h"
#include "hermes_logger.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

/* Case-insensitive substring search (portable; strcasestr is GNU-only). */
static const char *ci_strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t hlen = strlen(haystack), nlen = strlen(needle);
    if (nlen == 0 || nlen > hlen) return NULL;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
        }
        if (j == nlen) return haystack + i;
    }
    return NULL;
}

/* ================================================================
 *  1. Lazy OpenAI SDK proxy
 * ================================================================ */

/* PoP: load_openai_cls @ agent/auxiliary_client.py:_load_openai_cls */
/* PoP: load_openai_cls @ agent/process_bootstrap.py:_load_openai_cls */
const void *load_openai_cls(void) {
    /* C's OpenAI-compatible client is built into the provider system via
     * libhttp. The OpenAI Python SDK class is not instantiable from C. */
    hermes_log(LOG_DEBUG, "process_bootstrap",
               "load_openai_cls: OpenAI SDK not available in C, returning NULL");
    return NULL;
}

/* PoP: openai_proxy_is_instance @ agent/process_bootstrap.py:__instancecheck__ */
int openai_proxy_is_instance(const void *obj) {
    /* Python: isinstance(obj, openai.OpenAI). In C our OpenAI-compatible
     * client is an http_t created through the provider system; we accept any
     * non-NULL http_t as a member of the OpenAI-compatible family. */
    return obj != NULL;
}

/* PoP: openai_proxy_repr @ agent/process_bootstrap.py:__repr__ */
const char *openai_proxy_repr(void) {
    return "<lazy openai.OpenAI proxy>";
}

/* ================================================================
 *  2. Crash-resistant stdio (_SafeWriter)
 * ================================================================ */

struct safe_writer {
    FILE *inner;
};

/* PoP: safe_writer_create @ agent/process_bootstrap.py:__init__ */
safe_writer_t *safe_writer_create(FILE *inner) {
    if (!inner) return NULL;
    safe_writer_t *w = (safe_writer_t *)malloc(sizeof(*w));
    if (!w) return NULL;
    w->inner = inner;
    return w;
}

/* PoP: safe_writer_write @ agent/process_bootstrap.py:write */
size_t safe_writer_write(safe_writer_t *w, const char *data, size_t len) {
    if (!w || !w->inner || !data) return 0;
    errno = 0;
    size_t n = fwrite(data, 1, len ? len : strlen(data), w->inner);
    if (ferror(w->inner)) {
        /* Broken pipe / closed stream — swallow like the Python OSError/
         * ValueError guard. Return the requested length so callers that
         * check the write count stay happy. */
        clearerr(w->inner);
        return len ? len : strlen(data);
    }
    return n;
}

/* PoP: safe_writer_flush @ agent/process_bootstrap.py:flush */
void safe_writer_flush(safe_writer_t *w) {
    if (!w || !w->inner) return;
    if (fflush(w->inner) != 0) {
        if (errno == EPIPE || errno == EBADF) clearerr(w->inner);
    }
}

/* PoP: safe_writer_fileno @ agent/process_bootstrap.py:fileno */
int safe_writer_fileno(safe_writer_t *w) {
    if (!w || !w->inner) return -1;
    return fileno(w->inner);
}

/* PoP: safe_writer_isatty @ agent/process_bootstrap.py:isatty */
int safe_writer_isatty(safe_writer_t *w) {
    if (!w || !w->inner) return 0;
    int fd = fileno(w->inner);
    if (fd < 0) return 0;
    return isatty(fd);
}

/* PoP: safe_writer_inner @ agent/process_bootstrap.py:__getattr__ */
/* Forward access to the wrapped stream's own attributes. In C we expose the
 * underlying FILE* for the realistic attribute names; string/encoding-style
 * attributes are not meaningful and return NULL. */
FILE *safe_writer_inner(safe_writer_t *w) {
    return w ? w->inner : NULL;
}

/* Free the wrapper (does not close the underlying stream). */
void safe_writer_free(safe_writer_t *w) {
    free(w);
}

/* PoP: install_safe_stdio @ agent/process_bootstrap.py:_install_safe_stdio */
void install_safe_stdio(void) {
    /* Wrap stdout/stderr so best-effort console output cannot crash the
     * agent on a broken pipe. Idempotent: skip streams already wrapped. */
    static int installed = 0;
    if (installed) return;
    installed = 1;

    if (stdout && fileno(stdout) >= 0) {
        /* Heuristic: if stdout is already one of our wrappers we can't tell
         * from here, so just ensure SIGPIPE is ignored (the real protection). */
    }
    /* Ignore SIGPIPE so broken-pipe writes return EPIPE instead of killing
     * the process; safe_writer_write/flush swallow EPIPE explicitly. */
    signal(SIGPIPE, SIG_IGN);
    hermes_log(LOG_DEBUG, "process_bootstrap",
               "install_safe_stdio: SIGPIPE set to SIG_IGN (stdio is crash-resistant)");
}

/* ================================================================
 *  3. HTTP proxy / keepalive client
 * ================================================================ */

/* PoP: build_keepalive_http_client @ agent/process_bootstrap.py:build_keepalive_http_client */
struct http_t *build_keepalive_http_client(const char *base_url,
                                           int async_mode,
                                           int verify) {
    (void)async_mode; /* C client is synchronous; accepted for parity. */
    (void)verify;     /* libhttp honours system CA bundle; accepted for parity. */

    /* Copilot's api.githubcopilot.com endpoint: Python returns a bare client
     * with no custom transport / proxy. Mirror that — no proxy, default pool. */
    int is_copilot = (base_url && ci_strstr(base_url, "api.githubcopilot.com") != NULL);

    struct http_t *h = http_new(30);
    if (!h) return NULL;

    if (is_copilot) {
        /* No proxy for copilot; enable keep-alive pool. */
        http_client_set_proxy(h, NULL);
    } else {
        char *proxy = get_proxy_for_base_url(base_url);
        if (proxy) {
            http_client_set_proxy(h, proxy);
            free(proxy);
        }
    }
    /* HTTP-level keep-alive connection pool (the practical C equivalent of
     * the Python SO_KEEPALIVE socket options). */
    http_client_set_pool(h, 4, 60);
    return h;
}
