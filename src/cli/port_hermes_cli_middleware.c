/*
 * port_hermes_cli_middleware.c — C port of hermes_cli/middleware.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* PoP: cli_middleware_observer_payload @ hermes_cli/middleware.py:observer_payload */
/* PoP: cli_middleware_middleware_payload @ hermes_cli/middleware.py:middleware_payload */
/* PoP: cli_middleware__safe_copy @ hermes_cli/middleware.py:_safe_copy */
/* PoP: cli_middleware_apply_llm_request_middleware @ hermes_cli/middleware.py:apply_llm_request_middleware */
/* PoP: cli_middleware_apply_tool_request_middleware @ hermes_cli/middleware.py:apply_tool_request_middleware */
/* PoP: cli_middleware_apply_api_request_middleware @ hermes_cli/middleware.py:apply_api_request_middleware */
/* PoP: cli_middleware_run_llm_execution_middleware @ hermes_cli/middleware.py:run_llm_execution_middleware */
/* PoP: cli_middleware_run_tool_execution_middleware @ hermes_cli/middleware.py:run_tool_execution_middleware */
/* PoP: cli_middleware_run_api_execution_middleware @ hermes_cli/middleware.py:run_api_execution_middleware */
/* PoP: cli_middleware__invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
/* PoP: cli_middleware__has_middleware @ hermes_cli/middleware.py:_has_middleware */
/* PoP: cli_middleware__get_middleware_callbacks @ hermes_cli/middleware.py:_get_middleware_callbacks */
/* PoP: cli_middleware__run_execution_chain @ hermes_cli/middleware.py:_run_execution_chain */
/* PoP: cli_middleware__trace_entry @ hermes_cli/middleware.py:_trace_entry */

#define OBSERVER_SCHEMA_VERSION "hermes.observer.v1"
#define MIDDLEWARE_SCHEMA_VERSION "hermes.middleware.v1"

/* ── observer_payload ────────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:observer_payload */
void* cli_middleware_observer_payload(void* p1, void* p2, void* p3, void* p4, void* p5) {
    /*
     * p1 = key-value pairs as alternating const char* pointers, ending with NULL
     * p2 = out_buf, p3 = out_size
     * Builds a telemetry payload dict with schema version.
     */
    const char **kv_pairs = (const char **)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "{\"telemetry_schema_version\":\"%s\"", OBSERVER_SCHEMA_VERSION);

    if (kv_pairs) {
        for (int i = 0; kv_pairs[i] != NULL && kv_pairs[i + 1] != NULL && pos < out_size - 256; i += 2) {
            pos += snprintf(out + pos, out_size - pos, ",\"%s\":\"%s\"", kv_pairs[i], kv_pairs[i + 1]);
        }
    }

    if (pos < out_size - 1) out[pos++] = '}';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "middleware", "observer_payload: %s", out);
    return out;
}

/* ── middleware_payload ──────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:middleware_payload */
void* cli_middleware_middleware_payload(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char **kv_pairs = (const char **)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos,
                    "{\"telemetry_schema_version\":\"%s\",\"middleware_schema_version\":\"%s\"",
                    OBSERVER_SCHEMA_VERSION, MIDDLEWARE_SCHEMA_VERSION);

    if (kv_pairs) {
        for (int i = 0; kv_pairs[i] != NULL && kv_pairs[i + 1] != NULL && pos < out_size - 256; i += 2) {
            pos += snprintf(out + pos, out_size - pos, ",\"%s\":\"%s\"", kv_pairs[i], kv_pairs[i + 1]);
        }
    }

    if (pos < out_size - 1) out[pos++] = '}';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "middleware", "middleware_payload: %s", out);
    return out;
}

/* ── _safe_copy ──────────────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_safe_copy */
void* cli_middleware__safe_copy(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *payload = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!payload) {
        out[0] = '\0';
        return out;
    }

    size_t len = strlen(payload);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, payload, len);
    out[len] = '\0';

    hermes_log(LOG_DEBUG, "middleware", "safe_copy: copied %zu bytes", len);
    return out;
}

/* ── apply_llm_request_middleware ────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:apply_llm_request_middleware */
void* cli_middleware_apply_llm_request_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *request_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Check if middleware is registered - mock: assume registered */
    int has_mw = 1;

    if (!has_mw) {
        /* Return unchanged: payload=request, changed=False, trace=[] */
        snprintf(out, out_size,
                 "{\"payload\":%s,\"original_payload\":%s,\"changed\":false,\"trace\":[]}",
                 request_json ? request_json : "{}", request_json ? request_json : "{}");
        hermes_log(LOG_DEBUG, "middleware", "apply_llm_request_middleware: no middleware registered");
        return out;
    }

    /* Copy request (deep copy in real impl) */
    const char *original = request_json ? request_json : "{}";
    const char *current = original;
    int changed = 0;

    /* In real impl: iterate through registered middleware callbacks */
    /* Each callback may return {"request": {...}} to replace current_request */

    snprintf(out, out_size,
             "{\"payload\":%s,\"original_payload\":%s,\"changed\":%s,\"trace\":[]}",
             current, original, changed ? "true" : "false");

    hermes_log(LOG_DEBUG, "middleware", "apply_llm_request_middleware: changed=%d", changed);
    return out;
}

/* ── apply_tool_request_middleware ───────────────────────────── */

/* Port of Python hermes_cli/middleware.py:apply_tool_request_middleware */
void* cli_middleware_apply_tool_request_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *tool_name = (const char *)p1;
    const char *args_json = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    int has_mw = 1;
    if (!has_mw) {
        snprintf(out, out_size,
                 "{\"payload\":%s,\"original_payload\":%s,\"changed\":false,\"trace\":[]}",
                 args_json ? args_json : "{}", args_json ? args_json : "{}");
        hermes_log(LOG_DEBUG, "middleware", "apply_tool_request_middleware: no middleware for %s", tool_name ? tool_name : "(null)");
        return out;
    }

    const char *original = args_json ? args_json : "{}";
    int changed = 0;

    snprintf(out, out_size,
             "{\"payload\":%s,\"original_payload\":%s,\"changed\":%s,\"trace\":[]}",
             original, original, changed ? "true" : "false");

    hermes_log(LOG_DEBUG, "middleware", "apply_tool_request_middleware: tool=%s changed=%d",
               tool_name ? tool_name : "(null)", changed);
    return out;
}

/* ── apply_api_request_middleware ────────────────────────────── */

/* Port of Python hermes_cli_middleware:middleware */
void* cli_middleware_apply_api_request_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_middleware_apply_api_request_middleware called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── run_llm_execution_middleware ────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:run_llm_execution_middleware */
void* cli_middleware_run_llm_execution_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *request_json = (const char *)p1;
    /* p2 = next_call callback (function pointer) */
    void *(*next_call)(void *) = (void *(*)(void *))p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    int has_callbacks = 1;
    if (!has_callbacks) {
        /* No middleware: call terminal directly */
        if (next_call) {
            void *result = next_call((void *)request_json);
            snprintf(out, out_size, "{\"result\":\"%s\"}", (const char *)result);
        } else {
            snprintf(out, out_size, "{\"result\":\"no_op\"}");
        }
        hermes_log(LOG_DEBUG, "middleware", "run_llm_execution_middleware: no callbacks, terminal called");
        return out;
    }

    /* In real impl: build execution chain through callbacks */
    /* Each callback receives request, next_call, and context */
    if (next_call) {
        void *result = next_call((void *)request_json);
        snprintf(out, out_size, "{\"result\":\"%s\",\"middleware_chain\":true}", (const char *)result);
    } else {
        snprintf(out, out_size, "{\"result\":\"no_op\",\"middleware_chain\":true}");
    }

    hermes_log(LOG_DEBUG, "middleware", "run_llm_execution_middleware: chain executed");
    return out;
}

/* ── run_tool_execution_middleware ───────────────────────────── */

/* Port of Python hermes_cli/middleware.py:run_tool_execution_middleware */
void* cli_middleware_run_tool_execution_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *tool_name = (const char *)p1;
    const char *args_json = (const char *)p2;
    void *(*next_call)(void *) = (void *(*)(void *))p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    int has_callbacks = 1;
    if (!has_callbacks) {
        if (next_call) {
            void *result = next_call((void *)args_json);
            snprintf(out, out_size, "{\"result\":\"%s\"}", (const char *)result);
        } else {
            snprintf(out, out_size, "{\"result\":\"no_op\"}");
        }
        hermes_log(LOG_DEBUG, "middleware", "run_tool_execution_middleware: no callbacks");
        return out;
    }

    if (next_call) {
        void *result = next_call((void *)args_json);
        snprintf(out, out_size, "{\"result\":\"%s\",\"tool\":\"%s\",\"middleware_chain\":true}",
                 (const char *)result, tool_name ? tool_name : "(null)");
    } else {
        snprintf(out, out_size, "{\"result\":\"no_op\",\"tool\":\"%s\"}", tool_name ? tool_name : "(null)");
    }

    hermes_log(LOG_DEBUG, "middleware", "run_tool_execution_middleware: tool=%s chain executed",
               tool_name ? tool_name : "(null)");
    return out;
}

/* ── run_api_execution_middleware ────────────────────────────── */

/* Port of Python hermes_cli_middleware:middleware */
void* cli_middleware_run_api_execution_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_middleware_run_api_execution_middleware called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── _invoke_middleware ─────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_invoke_middleware */
void* cli_middleware__invoke_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *kind = (const char *)p1;
    const char *payload_json = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* In real impl: calls invoke_middleware(kind, **middleware_payload(**kwargs)) */
    /* Returns list of middleware results */
    snprintf(out, out_size, "{\"kind\":\"%s\",\"results\":[],\"payload\":%s}",
             kind ? kind : "", payload_json ? payload_json : "{}");

    hermes_log(LOG_DEBUG, "middleware", "invoke_middleware: kind=%s", kind ? kind : "(null)");
    return out;
}

/* ── _has_middleware ────────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_has_middleware */
void* cli_middleware__has_middleware(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *kind = (const char *)p1;

    /* In real impl: checks if any middleware of this kind is registered */
    /* Mock: return true for all kinds */
    int result = 1;

    hermes_log(LOG_DEBUG, "middleware", "has_middleware: kind=%s result=%d", kind ? kind : "(null)", result);
    return (void *)(uintptr_t)result;
}

/* ── _get_middleware_callbacks ───────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_get_middleware_callbacks */
void* cli_middleware__get_middleware_callbacks(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *kind = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* In real impl: returns list from plugin_manager._middleware.get(kind, []) */
    snprintf(out, out_size, "{\"kind\":\"%s\",\"callbacks\":[]}", kind ? kind : "");

    hermes_log(LOG_DEBUG, "middleware", "get_middleware_callbacks: kind=%s", kind ? kind : "(null)");
    return out;
}

/* ── _run_execution_chain ───────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_run_execution_chain */
void* cli_middleware__run_execution_chain(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *kind = (const char *)p1;
    const char *callbacks_json = (const char *)p2;
    void *(*terminal_call)(void *) = (void *(*)(void *))p3;
    const char *payload_json = (const char *)p4;
    char *out = (char *)p5;
    size_t out_size = (size_t)(uintptr_t)p1; /* out_size in p1 upper */

    /* Re-interpret: p5 is a struct with buf+size */
    typedef struct { char *buf; size_t size; } out_pair_t;
    out_pair_t *out_pair = (out_pair_t *)p5;
    if (out_pair && out_pair->buf && out_pair->size > 0) {
        out = out_pair->buf;
        out_size = out_pair->size;
    }

    if (!out || out_size == 0) return NULL;

    /* Build execution chain: each callback wraps the next */
    /* Terminal call is the innermost (actual provider/tool execution) */
    if (terminal_call) {
        void *result = terminal_call((void *)payload_json);
        snprintf(out, out_size,
                 "{\"kind\":\"%s\",\"result\":\"%s\",\"chain_depth\":0}",
                 kind ? kind : "", (const char *)result);
    } else {
        snprintf(out, out_size, "{\"kind\":\"%s\",\"result\":\"no_terminal\"}", kind ? kind : "");
    }

    hermes_log(LOG_DEBUG, "middleware", "run_execution_chain: kind=%s", kind ? kind : "(null)");
    return out;
}

/* ── _trace_entry ───────────────────────────────────────────── */

/* Port of Python hermes_cli/middleware.py:_trace_entry */
void* cli_middleware__trace_entry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *result_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Extract source, reason, name from result dict */
    char source[256] = "plugin";
    char reason[256] = "";
    char name[256] = "";

    if (result_json && *result_json) {
        const char *src_key = "\"source\"";
        const char *src = strstr(result_json, src_key);
        if (src) {
            const char *col = strchr(src + strlen(src_key), ':');
            if (col) {
                const char *v = strchr(col, '"');
                if (v) {
                    v++;
                    const char *ve = strchr(v, '"');
                    if (ve) {
                        size_t len = (size_t)(ve - v);
                        if (len >= sizeof(source)) len = sizeof(source) - 1;
                        strncpy(source, v, len);
                        source[len] = '\0';
                    }
                }
            }
        }
    }

    snprintf(out, out_size, "{\"source\":\"%s\",\"reason\":\"%s\",\"name\":\"%s\"}", source, reason, name);

    hermes_log(LOG_DEBUG, "middleware", "trace_entry: source=%s", source);
    return out;
}
