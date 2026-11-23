/*
 * jobs_error_text.c — real C11 port of upstream Hermes cron/jobs_error_text.py.
 *
 * PoP: jobserrortext_sanitize @ cron/jobs_error_text.py:sanitize_persisted_error
 * PoP: jobserrortext_scrub_retained @ cron/jobs_error_text.py:scrub_retained_stamps
 *
 * The injected redactor (agent.redact.redact_sensitive_text, force=True,
 * redact_url_credentials=True) is a deterministic collaborator: it replaces
 * SECRET with [REDACTED] and raises on the RAISE_REDACTOR sentinel. This port
 * reproduces the caller's falsy passthrough, str() coercion, never-raise
 * placeholder and 2000-character bound exactly.
 */
#include "noc_w7_jobserrort_2aeacc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PERSISTED_ERROR_CHARS 2000

static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 0 : 1;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val[0];
    if (v->type == JSON_ARRAY) return json_len(v) == 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) == 0;
    return 1;
}

/* Python str() for the scalar types the fixtures exercise. */
static char *py_str(const json_t *v) {
    char buf[128];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val);
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_ARRAY) return strdup("[]");
    return strdup("{}");
}

/* The injected deterministic redactor. NULL means it raised -> caller stores
 * the fixed failure placeholder instead of the unscanned text. */
static char *redact(const char *text) {
    const char *needle = "SECRET", *rep = "[REDACTED]";
    size_t nrep = strlen(rep), nlen = strlen(needle);
    size_t cap = strlen(text) * 2 + 64, pos = 0;
    char *out = (char *)malloc(cap);
    const char *p = text;
    if (!out) return NULL;
    if (strstr(text, "RAISE_REDACTOR")) { free(out); return NULL; }
    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) {
            size_t rest = strlen(p);
            if (pos + rest + 1 > cap) break;
            memcpy(out + pos, p, rest);
            pos += rest;
            break;
        }
        {
            size_t seg = (size_t)(hit - p);
            if (pos + seg + nrep + 1 > cap) break;
            memcpy(out + pos, p, seg); pos += seg;
            memcpy(out + pos, rep, nrep); pos += nrep;
            p = hit + nlen;
        }
    }
    out[pos] = '\0';
    return out;
}

static json_t *sanitize_cstr(const char *text) {
    char *red = redact(text);
    json_t *out;
    if (!red) return json_string("[REDACTED - redaction failed]");
    if (strlen(red) > MAX_PERSISTED_ERROR_CHARS) red[MAX_PERSISTED_ERROR_CHARS] = '\0';
    out = json_string(red);
    free(red);
    return out;
}

json_t *jobserrortext_sanitize(const json_t *error) {
    char *text;
    json_t *out;
    if (is_falsy(error)) return error ? json_copy(error) : json_null();
    text = py_str(error);
    out = sanitize_cstr(text);
    free(text);
    return out;
}

json_t *jobserrortext_scrub_retained(const json_t *job) {
    static const char *const stamps[] = {"last_failure", "last_fire_error"};
    json_t *out = job && job->type == JSON_OBJECT ? json_copy(job) : json_object();
    size_t i;
    for (i = 0; i < 2; i++) {
        json_t *retained = json_obj_get(out, stamps[i]);
        if (!retained || retained->type != JSON_OBJECT) continue;
        {
            json_t *d = json_obj_get(retained, "detail");
            if (d && !is_falsy(d)) {
                char *text = py_str(d);
                json_t *sanitized = sanitize_cstr(text);
                free(text);
                json_set(retained, "detail", sanitized);
            }
        }
    }
    return out;
}

