/*
 * run_turn.c — faithful C11 port of the pure helpers in upstream Hermes
 * gateway/run_turn.py (gate w6_runturn_6f520a).
 *
 * PoP: w6_runturn_is_context_overflow    @ gateway/run_turn.py:is_context_overflow_failure_result
 * PoP: w6_runturn_bound_model_input      @ gateway/run_turn.py:bound_model_input_without_hygiene
 * PoP: w6_runturn_hygiene_no_commit_reason @ gateway/run_turn.py:hygiene_no_commit_reason
 *
 * The three ported functions are the module's deterministic, side-effect-free
 * helpers.  The async turn-running methods are effectful and out of scope.
 */
#include "noc_w6_runturn_6f520a.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a JSON value. */
static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default:          return 0;
    }
}

/* Python str() coercion for the JSON scalar values the helpers f-string. */
static char *py_str(const json_t *v) {
    char buf[64];
    char *s;
    if (!v || v->type == JSON_NULL) return strdup("");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (isfinite(v->num_val) && floor(v->num_val) == v->num_val
                && v->num_val >= -9.0e18 && v->num_val <= 9.0e18) {
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            } else {
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            }
            return strdup(buf);
        default:
            s = json_dumps(v, 0);
            return s ? s : strdup("");
    }
}

static void lower_ascii(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* helper: object -> member, or NULL */
static json_t *member(const json_t *o, const char *key) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, key) : NULL;
}

/* helper: is `v` a dict whose "role" string equals `role`? */
static int role_is(const json_t *v, const char *role) {
    json_t *r = member(v, "role");
    return (v && v->type == JSON_OBJECT && r && r->type == JSON_STRING
            && r->str_val && !strcmp(r->str_val, role));
}

int w6_runturn_is_context_overflow(const json_t *result, long long history_len) {
    if (!py_truthy(member(result, "failed"))) return 0;
    if (py_truthy(member(result, "compression_exhausted"))) return 1;

    json_t *errv = member(result, "error");
    /* str(agent_result.get("error") or "") */
    char *err = py_truthy(errv) ? py_str(errv) : strdup("");
    lower_ascii(err);

    static const char *const phrases[] = {
        "context length", "context size", "context window",
        "maximum context", "token limit", "too many tokens",
        "reduce the length", "exceeds the limit",
        "request entity too large", "prompt is too long",
        "payload too large", "input is too long",
    };
    int verdict = 0;
    for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); i++) {
        if (strstr(err, phrases[i]) != NULL) { verdict = 1; break; }
    }
    if (!verdict && strstr(err, "400") != NULL && history_len > 50) verdict = 1;
    free(err);
    return verdict;
}

json_t *w6_runturn_bound_model_input(const json_t *history, long long limit) {
    size_t n = (history && history->type == JSON_ARRAY) ? history->c.count : 0;
    if ((long long)n <= limit) return json_copy(history ? history : json_array());

    long long head_end = 0;
    while (head_end < (long long)n && history->c.items[head_end]
           && history->c.items[head_end]->type == JSON_OBJECT) {
        json_t *row = history->c.items[head_end];
        json_t *r = member(row, "role");
        if (!(r && r->type == JSON_STRING && r->str_val
              && (!strcmp(r->str_val, "system") || !strcmp(r->str_val, "session_meta"))))
            break;
        head_end++;
    }
    long long he = head_end;
    if (he > limit - 1) he = limit - 1;  /* min(head_end, limit - 1) */

    long long tail_start = (long long)n - (limit - he);
    while (tail_start < (long long)n && history->c.items[tail_start]
           && role_is(history->c.items[tail_start], "tool"))
        tail_start++;

    json_t *out = json_array();
    for (long long i = 0; i < he; i++)
        json_append(out, json_copy(history->c.items[i]));
    for (long long i = tail_start; i < (long long)n; i++)
        json_append(out, json_copy(history->c.items[i]));
    return out;
}

char *w6_runturn_hygiene_no_commit_reason(const json_t *attrs) {
    if (!py_truthy(member(attrs, "_last_compression_attempt_recorded")))
        return strdup("compression did not run");

    json_t *lock_skip = member(attrs, "_compression_skipped_due_to_lock");
    if ((lock_skip && lock_skip->type == JSON_BOOL && lock_skip->bool_val)
        || (lock_skip && lock_skip->type == JSON_STRING))
        return strdup("attempt skipped: compression lease held by another process");

    json_t *blocked = member(attrs, "_compression_blocked_transient");
    if (py_truthy(blocked)) {
        char *b = py_str(blocked);
        size_t cap = strlen(b) + 64;
        char *out = (char *)malloc(cap);
        snprintf(out, cap, "attempt blocked: %s", b);
        free(b);
        return out;
    }

    json_t *in_place = member(attrs, "_last_compression_attempt_in_place");
    if (!in_place || in_place->type == JSON_NULL) {
        const char *detail = py_truthy(member(attrs, "_last_compression_timed_out"))
                             ? "summary timed out" : "aborted before commit";
        json_t *warning = member(attrs, "_last_compression_summary_warning");
        if (py_truthy(warning)) {
            char *w = py_str(warning);
            size_t cap = strlen(detail) + strlen(w) + 64;
            char *out = (char *)malloc(cap);
            snprintf(out, cap, "attempt %s: %s", detail, w);
            free(w);
            return out;
        }
        size_t cap = strlen(detail) + 64;
        char *out = (char *)malloc(cap);
        snprintf(out, cap, "attempt %s", detail);
        return out;
    }

    json_t *db = member(attrs, "_session_db");
    if (!db || db->type == JSON_NULL)
        return strdup("no session_db on the hygiene agent");

    return strdup("in-place commit did not complete");
}
