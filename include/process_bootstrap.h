/*
 * process_bootstrap.h — public API for agent/process_bootstrap.py port.
 *
 * Opaque types only; implementation lives in src/agent/process_bootstrap.c.
 * Kept minimal (no god-header): just the bootstrap/stdio-proxy surface.
 */
#ifndef HERMES_PROCESS_BOOTSTRAP_H
#define HERMES_PROCESS_BOOTSTRAP_H

#include <stdio.h>

/* Crash-resistant stdio wrapper (Python _SafeWriter).
 * Wraps a FILE* and swallows broken-pipe / closed-stream errors on
 * write/flush/isatty so best-effort console output cannot crash the agent. */
typedef struct safe_writer safe_writer_t;

/* Create a safe writer around an existing stream (takes no ownership). */
safe_writer_t *safe_writer_create(FILE *inner);

/* Wrap the process stdout/stderr with safe writers (idempotent). */
void install_safe_stdio(void);

/* Delegated stream ops (all noexcept). */
size_t safe_writer_write(safe_writer_t *w, const char *data, size_t len);
void   safe_writer_flush(safe_writer_t *w);
int    safe_writer_fileno(safe_writer_t *w);
int    safe_writer_isatty(safe_writer_t *w);
/* Free the wrapper (does NOT close the underlying stream). */
void   safe_writer_free(safe_writer_t *w);

/* Forwarded attributes (Python __getattr__ on the inner stream).
 * Returns a borrowed FILE* for "inner"/"buffer"/"raw" or NULL otherwise.
 * Use safe_writer_forward_int() for numeric-ish attributes (encoding etc.
 * are string-typed and not meaningful in C — return NULL there). */
FILE  *safe_writer_inner(safe_writer_t *w);

/* Lazy OpenAI-SDK proxy (Python _OpenAIProxy).
 * C has no OpenAI Python SDK; the proxy reports instance-membership against
 * our OpenAI-compatible http client and a stable repr. */
int  openai_proxy_is_instance(const void *obj);
const char *openai_proxy_repr(void);

/* Build a keepalive HTTP client honouring env proxy / NO_PROXY policy.
 * Returns an http_t* (caller frees) or NULL on failure. async_mode / verify
 * are accepted for signature parity but the C client is synchronous. */
struct http_t *build_keepalive_http_client(const char *base_url,
                                           int async_mode,
                                           int verify);

#endif /* HERMES_PROCESS_BOOTSTRAP_H */
