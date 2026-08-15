/* Slermes C port — gateway/run.py (DA-4 gaps → real implementations)
 * Gateway loop helpers, env parsing, error classification */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── hygiene_compaction_recovered (L207) ── */
/* PoP: gw_run_hygiene_compaction_recovered @ gateway/run.py:hygiene_compaction_recovered */
bool gw_run_hygiene_compaction_recovered(bool aborted, bool rotated, bool in_place,
                                         int msg_count, int new_count,
                                         int approx_tokens, int new_tokens) {
    if (aborted) return false;
    if (new_count < msg_count) return true;
    if (new_tokens < approx_tokens * 0.9) return true;
    return false;
}

/* ── _float_env (L1044) ── */
/* PoP: gw_run_float_env @ gateway/run.py:_float_env */
double gw_run_float_env(const char *name, double default_val) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end && *end == '\0') return val;
    return default_val;
}

/* ── _looks_like_gateway_provider_error (L696) ── */
/* PoP: gw_run_looks_like_provider_error @ gateway/run.py:_looks_like_gateway_provider_error */
bool gw_run_looks_like_provider_error(const char *text) {
    if (!text || !*text) return false;
    size_t len = strlen(text);
    if (len > 400) return false;
    int newlines = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') newlines++;
    }
    if (newlines > 4) return false;
    /* Check for common error markers at start */
    if (strstr(text, "HTTP ") || strstr(text, "Error:") || strstr(text, "error:") ||
        strstr(text, "Request failed") || strstr(text, "Traceback")) {
        return true;
    }
    return false;
}

/* ── _csv_or_list_to_set (L1293) ── */
/* PoP: gw_run_csv_or_list_to_set @ gateway/run.py:_csv_or_list_to_set */
json_t *gw_run_csv_or_list_to_set(const char *raw) {
    json_t *result = json_array();
    if (!raw || !*raw) return result;
    const char *p = raw;
    while (*p) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        /* Find end of token */
        const char *start = p;
        while (*p && *p != ',') p++;
        size_t len = p - start;
        /* Trim trailing whitespace */
        while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\t')) len--;
        if (len > 0) {
            char *token = malloc(len + 1);
            if (token) {
                memcpy(token, start, len);
                token[len] = '\0';
                json_append(result, json_string(token));
                free(token);
            }
        }
    }
    return result;
}

/* ── _non_conversational_metadata (L472) ── */
/* PoP: gw_run_non_conversational_metadata @ gateway/run.py:_non_conversational_metadata */
json_t *gw_run_non_conversational_metadata(json_t *metadata, const char *platform) {
    if (!metadata || metadata->type != JSON_OBJECT) {
        metadata = json_object();
    }
    if (platform && strcmp(platform, "discord") == 0) {
        json_set(metadata, "non_conversational", json_bool(true));
    }
    return metadata;
}
