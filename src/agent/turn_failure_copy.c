/*
 * turn_failure_copy.c - C home for upstream Hermes agent/turn_failure_copy.py.
 *
 * PoP: w9_turnfailure_a171e6_stamp_failure @ agent/turn_failure_copy.py:stamp_failure
 * PoP: w9_turnfailure_a171e6_untyped_failed_turn_display_kind @ agent/turn_failure_copy.py:untyped_failed_turn_display_kind
 * PoP: w9_turnfailure_a171e6_failed_turn_notice @ agent/turn_failure_copy.py:failed_turn_notice
 * PoP: w9_turnfailure_a171e6_exit_reason_failure @ agent/turn_failure_copy.py:exit_reason_failure
 * PoP: w9_turnfailure_a171e6_is_max_iteration_handoff @ agent/turn_failure_copy.py:is_max_iteration_handoff
 * PoP: w9_turnfailure_a171e6_failure_cause_gloss @ agent/turn_failure_copy.py:failure_cause_gloss
 * PoP: w9_turnfailure_a171e6_short_detail @ agent/turn_failure_copy.py:short_detail
 * PoP: w9_turnfailure_a171e6_invalid_response_failure_reason @ agent/turn_failure_copy.py:invalid_response_failure_reason
 *
 * Module literals are mirrored verbatim from the Python source; the oracle seeds
 * the same FailoverReason values when extracting the live functions.
 */
#include "noc_w9_turnfailure_a171e6.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TF_FAILED "Your request was not processed. Send it again if you still want me to carry it out."
#define TF_PARTIAL "This turn did not complete. Some actions may already have run; verify their effects before resending."
#define TF_AUTOMATION "An automated delivery was interrupted and was not run again. Some actions may already have run; verify their effects."
#define TF_DISPLAY_KIND "failed_turn"

static int tf_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count ? 1 : 0;
    }
}

static char *tf_strip(const char *s) {
    while (s && isspace((unsigned char)*s)) s++;
    size_t n = s ? strlen(s) : 0;
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    memcpy(out, s ? s : "", n);
    out[n] = '\0';
    return out;
}

/* (exit-reason prefix, failure_reason, retryable, fails_turn) */
static const struct { const char *prefix, *code; int retryable, fails_turn; } TF_EXIT_FAILURES[] = {
    {"empty_response_exhausted", "empty_response", 1, 0},
    {"all_retries_exhausted_no_response", "server_error", 1, 1},
    {"pending_tool_result", "loop_error", 1, 1},
    {"interpreter_shutdown", "interpreter_shutdown", 0, 1},
    {"local_processing_error", "loop_error", 0, 0},
    {"repeated_outer_errors", "loop_error", 1, 1},
    {"error_near_max_iterations", "loop_error", 1, 1},
    {"context_compression_timeout", "context_overflow", 0, 1},
    {"context_compression_exhausted", "context_overflow", 0, 1},
    {"ollama_runtime_context_too_small", "context_overflow", 0, 1},
    {"redirect_restart_limit_exceeded", "loop_error", 1, 0},
    {"rebuilt_restart_limit_exceeded", "loop_error", 1, 0},
};

json_t *w9_turnfailure_a171e6_stamp_failure(json_t *result, const char *reason, int retryable) {
    if (!result) return json_object();
    json_t *out = json_copy(result);
    json_set(out, "failure_reason", json_string(reason ? reason : ""));
    json_set(out, "failure_retryable", json_bool(retryable ? true : false));
    return out;
}

json_t *w9_turnfailure_a171e6_untyped_failed_turn_display_kind(const char *role, json_t *content) {
    if (role && !strcmp(role, "assistant") && content && content->type == JSON_STRING) {
        char *s = tf_strip(content->str_val ? content->str_val : "");
        int hit = !strcmp(s, TF_FAILED) || !strcmp(s, TF_PARTIAL) || !strcmp(s, TF_AUTOMATION);
        free(s);
        if (hit) return json_string(TF_DISPLAY_KIND);
    }
    return json_null();
}

json_t *w9_turnfailure_a171e6_failed_turn_notice(json_t *turn_messages) {
    if (json_is_array(turn_messages)) {
        for (size_t i = 0; i < json_len(turn_messages); ++i) {
            json_t *row = json_get(turn_messages, i);
            if (!json_is_object(row)) continue;
            const char *role = json_get_str(row, "role", "");
            if (!strcmp(role, "tool") ||
                (!strcmp(role, "assistant") && tf_truthy(json_obj_get(row, "tool_calls")))) {
                return json_string(TF_PARTIAL);
            }
        }
    }
    return json_string(TF_FAILED);
}

json_t *w9_turnfailure_a171e6_exit_reason_failure(const char *reason) {
    const char *r = reason ? reason : "";
    for (size_t i = 0; i < sizeof(TF_EXIT_FAILURES) / sizeof(TF_EXIT_FAILURES[0]); ++i) {
        if (!strncmp(r, TF_EXIT_FAILURES[i].prefix, strlen(TF_EXIT_FAILURES[i].prefix))) {
            json_t *out = json_object();
            json_set(out, "reason", json_string(TF_EXIT_FAILURES[i].code));
            json_set(out, "retryable", json_bool(TF_EXIT_FAILURES[i].retryable ? true : false));
            json_set(out, "fails_turn", json_bool(TF_EXIT_FAILURES[i].fails_turn ? true : false));
            return out;
        }
    }
    return json_null();
}

int w9_turnfailure_a171e6_is_max_iteration_handoff(json_t *result) {
    if (!json_is_object(result)) return 0;
    if (json_get_bool(result, "failed", false) || json_get_bool(result, "interrupted", false)) return 0;
    json_t *completed = json_obj_get(result, "completed");
    if (!completed || completed->type != JSON_BOOL || completed->bool_val != false) return 0;
    const char *reason = json_get_str(result, "turn_exit_reason", "");
    if (strncmp(reason, "max_iterations_reached(", 23)) return 0;
    char *summary = tf_strip(json_get_str(result, "final_response", ""));
    int answer = summary[0] ? 1 : 0;
    free(summary);
    return answer;
}

/* reason -> template (with {subject} / {possessive} placeholders) */
static const struct { const char *reason, *tmpl; } TF_GLOSS[] = {
    {"timeout", "the AI model service did not respond in time"},
    {"rate_limit", "the AI model service was rate-limited (too many requests)"},
    {"upstream_rate_limit", "the AI model service was rate-limited (too many requests)"},
    {"overloaded", "the AI model service is overloaded right now"},
    {"server_error", "the AI model service returned an internal error"},
    {"billing", "the AI model service says the account's usage or credit limit is reached"},
    {"billing_unverified", "the AI model service says the account's usage or credit limit is reached"},
    {"auth", "the AI model service rejected the sign-in"},
    {"auth_permanent", "the AI model service rejected the sign-in"},
    {"upstream_blocked", "a firewall/CDN in front of the AI model service blocked the request"},
    {"guardrail_blocked", "the AI model service's prompt-injection guardrail rejected the request"},
    {"model_not_found", "the model {subject} uses was not found at the AI model service"},
    {"content_policy_blocked", "the AI model service's safety filter rejected the request"},
    {"provider_policy_blocked", "the AI model service refused the request because of a policy on the account"},
    {"data_policy_denied", "{possessive} context holds a connected-app result that needs a model that does not train on data"},
    {"context_overflow", "{possessive} request grew too large for the model"},
    {"payload_too_large", "{possessive} request grew too large for the model"},
};

static char *tf_format(const char *tmpl, const char *subject, const char *possessive) {
    size_t cap = strlen(tmpl) * 2 + 64;
    char *out = (char *)calloc(cap + 1, 1);
    size_t j = 0;
    for (const char *p = tmpl; *p; ++p) {
        if (!strncmp(p, "{subject}", 9)) {
            j += (size_t)snprintf(out + j, cap - j, "%s", subject ? subject : "");
            p += 8;
        } else if (!strncmp(p, "{possessive}", 12)) {
            j += (size_t)snprintf(out + j, cap - j, "%s", possessive ? possessive : "");
            p += 11;
        } else {
            out[j++] = *p;
        }
    }
    out[j] = '\0';
    return out;
}

json_t *w9_turnfailure_a171e6_failure_cause_gloss(const char *reason,
                                                  const char *subject, const char *possessive) {
    const char *r = reason ? reason : "";
    for (size_t i = 0; i < sizeof(TF_GLOSS) / sizeof(TF_GLOSS[0]); ++i) {
        if (!strcmp(r, TF_GLOSS[i].reason)) {
            char *s = tf_format(TF_GLOSS[i].tmpl, subject ? subject : "it",
                                possessive ? possessive : "its");
            json_t *out = json_string(s);
            free(s);
            return out;
        }
    }
    return json_null();
}

json_t *w9_turnfailure_a171e6_short_detail(const char *text, int limit) {
    char *stripped = tf_strip(text ? text : "");
    for (char *p = stripped; *p; ++p) {
        if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
    }
    const char *first = stripped;
    if (!*first) first = "str";
    size_t n = strlen(first);
    json_t *out;
    if ((int)n <= limit) {
        out = json_string(first);
    } else {
        size_t keep = (size_t)(limit - 1);
        char *buf = (char *)malloc(keep + 5);
        memcpy(buf, first, keep);
        memcpy(buf + keep, "\xe2\x80\xa6", 3);  /* U+2026 */
        buf[keep + 3] = '\0';
        out = json_string(buf);
        free(buf);
    }
    free(stripped);
    return out;
}

json_t *w9_turnfailure_a171e6_invalid_response_failure_reason(json_t *error) {
    const char *fb = "invalid_response";
    if (!error) return json_string(fb);
    json_t *code = NULL;
    if (error->type == JSON_OBJECT) code = json_obj_get(error, "code");
    if (!code || code->type == JSON_NULL) return json_string(fb);
    long c;
    if (code->type == JSON_NUMBER) {
        c = (long)code->num_val;
    } else if (code->type == JSON_STRING) {
        char *end = NULL;
        c = strtol(code->str_val ? code->str_val : "", &end, 10);
        if (!end || *end) return json_string(fb);
    } else if (code->type == JSON_BOOL) {
        c = code->bool_val ? 1 : 0;
    } else {
        return json_string(fb);
    }
    switch (c) {
        case 429: return json_string("rate_limit");
        case 500: case 502: return json_string("server_error");
        case 503: case 529: return json_string("overloaded");
        case 504: case 524: return json_string("timeout");
        default: return json_string(fb);
    }
}
