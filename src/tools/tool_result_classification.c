/*
 * tool_result_classification.c — REAL implementation (was a name-parity stub).
 * Port of Python agent/tool_result_classification.py: classify a tool result as
 * retryable (transient) vs permanent so the agent loop can decide to retry.
 *
 * See tool_result_classification.h for the contract.
 */
#include "tool_result_classification.h"
#include "hermes_json.h"   /* libjson: json_parse / json_get_str / json_free */
#include <string.h>
#include <strings.h>       /* strcasestr */
#include <stdio.h>         /* snprintf */
#include <stddef.h>        /* memrchr guard */

/* Keywords that indicate a TRANSIENT (retryable) failure. */
static const char *kTransient[] = {
    "timeout", "timed out", "etimedout", "econn", "connection reset",
    "connection refused", "connection aborted", "network", "temporary",
    "try again", "rate limit", "rate_limit", "too many requests",
    "503", "502", "504", "429", "service unavailable", "bad gateway",
    "gateway timeout", "internal server error", "busy", "overloaded",
    "deadline", "eagain", "econnreset", "socket", "dns", NULL
};

/* Keywords that indicate a PERMANENT (do-not-retry) failure. */
static const char *kPermanent[] = {
    "not found", "no such", "404", "permission", "denied", "forbidden",
    "401", "403", "unauthorized", "unauthenticated", "invalid", "malformed",
    "400", "422", "bad request", "unprocessable", "illegal", "unknown tool",
    "missing required", "schema", "validation", "conflict", "409", NULL
};

static bool msg_has_any(const char *msg, const char **kw) {
    if (!msg) return false;
    for (size_t i = 0; kw[i]; i++) {
        if (strcasestr(msg, kw[i])) return true;
    }
    return false;
}

bool tool_result_is_error(const char *result_json) {
    if (!result_json) return false;
    /* LIVE shape is {"error": ...}. Allow optional leading whitespace. */
    const char *p = result_json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return strncmp(p, "{\"error\"", 7) == 0;
}

const char *tool_error_recovery_hint(const char *result_json) {
    static const char *empty = "";
    if (!result_json) return empty;
    json_node_t *root = json_parse(result_json, NULL);
    if (!root) return empty;
    const char *hint = json_get_str(root, "recovery_hint", "");
    if (!hint) hint = empty;
    /* Note: returned pointer is owned by root; caller uses before freeing. */
    /* We cannot keep root alive, so copy via a static buffer is unsafe for
     * concurrency; instead return the literal only when non-empty and caller
     * copies. To stay correct+simple, dup into a static thread-local. */
    static __thread char buf[256];
    if (hint && *hint) {
        snprintf(buf, sizeof(buf), "%s", hint);
        json_free(root);
        return buf;
    }
    json_free(root);
    return empty;
}

tool_error_class_t tool_classify_error(const char *result_json) {
    if (!tool_result_is_error(result_json)) return TOOL_ERR_NONE;

    json_node_t *root = json_parse(result_json, NULL);
    if (!root) return TOOL_ERR_PERMANENT; /* unparseable error -> fail closed */

    /* 1) Authoritative explicit signal. */
    const char *retry_raw = json_get_str(root, "retryable", NULL);
    if (retry_raw) {
        bool is_transient = (strcmp(retry_raw, "true") == 0);
        json_free(root);
        return is_transient ? TOOL_ERR_TRANSIENT : TOOL_ERR_PERMANENT;
    }

    /* 2) Heuristic on message + code + status. */
    const char *msg = json_get_str(root, "error", NULL);
    const char *code = json_get_str(root, "code", NULL);
    const char *status = json_get_str(root, "status", NULL);

    /* transient wins first only if no permanent keyword present (avoid
     * misclassifying "connection refused: permission denied"). */
    bool has_perm = msg_has_any(msg, kPermanent)
                 || msg_has_any(code, kPermanent)
                 || msg_has_any(status, kPermanent);
    if (has_perm) { json_free(root); return TOOL_ERR_PERMANENT; }

    bool has_trans = msg_has_any(msg, kTransient)
                 || msg_has_any(code, kTransient)
                 || msg_has_any(status, kTransient);
    if (has_trans) { json_free(root); return TOOL_ERR_TRANSIENT; }

    json_free(root);
    /* 3) Default fail-closed: do NOT retry unknown errors. */
    return TOOL_ERR_PERMANENT;
}

const char *tool_error_class_name(tool_error_class_t c) {
    switch (c) {
        case TOOL_ERR_NONE:      return "none";
        case TOOL_ERR_TRANSIENT: return "transient";
        case TOOL_ERR_PERMANENT: return "permanent";
    }
    return "unknown";
}

char *tool_error_with_class(const char *result_json, tool_error_class_t c) {
    if (!result_json) return NULL;
    if (!tool_result_is_error(result_json)) {
        return strdup(result_json);   /* not an error: pass through unchanged */
    }
    /* Parse, inject _retry_class, re-serialize. libjson has no re-serialize of
     * an arbitrary node, so append the field textually at the end of the object
     * (before the closing "}"). Robust for the LIVE error shape. */
    const char *cls = tool_error_class_name(c);
    size_t n = strlen(result_json);
    /* find last '}' (manual backward scan; memrchr needs _GNU_SOURCE) */
    const char *end = NULL;
    for (size_t i = n; i-- > 0; ) {
        if (result_json[i] == '}') { end = result_json + i; break; }
    }
    if (!end) return strdup(result_json);
    size_t pos = (size_t)(end - result_json);
    bool need_comma = (pos >= 2 && result_json[pos - 1] != '{'
                      && (result_json[pos - 1] != '"')); /* crude: assume body */
    /* Simpler: if the object has existing fields (the char before } is not '{'),
     * we need a comma. Detect by scanning: if there's a ':' before the last '}'. */
    bool has_fields = (memchr(result_json, ':', pos) != NULL);
    size_t outlen = pos + (has_fields ? 1 : 0) + strlen("\"_retry_class\":\"\"") + strlen(cls) + 1 + 1;
    char *out = (char *)malloc(outlen);
    if (!out) return NULL;
    size_t w = 0;
    memcpy(out, result_json, pos); w += pos;
    if (has_fields) { out[w++] = ','; }
    w += (size_t)snprintf(out + w, outlen - w, "\"_retry_class\":\"%s\"", cls);
    out[w++] = '}';
    out[w] = '\0';
    return out;
}
