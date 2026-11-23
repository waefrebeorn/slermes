/*
 * auxiliary_error_policy.c — C home for upstream Hermes agent/auxiliary_error_policy.py.
 *
 * PoP: w15_auxiliarye_905344_same_provider_retry @ agent/auxiliary_error_policy.py:same_provider_retry
 * PoP: w15_auxiliarye_905344_auth_refresh @ agent/auxiliary_error_policy.py:auth_refresh
 * PoP: w15_auxiliarye_905344_same_key_retry @ agent/auxiliary_error_policy.py:same_key_retry
 * PoP: w15_auxiliarye_905344_resend_cannot_help @ agent/auxiliary_error_policy.py:resend_cannot_help
 * PoP: w15_auxiliarye_905344_benches_provider @ agent/auxiliary_error_policy.py:benches_provider
 * PoP: w15_auxiliarye_905344_stream_negotiation_reraises @ agent/auxiliary_error_policy.py:stream_negotiation_reraises
 * PoP: w15_auxiliarye_905344_fallback_label @ agent/auxiliary_error_policy.py:fallback_label
 * PoP: w15_auxiliarye_905344_ladder_accepts @ agent/auxiliary_error_policy.py:ladder_accepts
 *
 * FailoverReason values are the enum .value strings; the classifier itself is
 * upstream, so the oracle supplies the verdict fields verbatim.
 */
#include "noc_w15_auxiliarye_905344.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const AUTH_ERROR = "auth error";
static const char *const PAYMENT_ERROR = "payment error";
static const char *const MODEL_INCOMPATIBLE = "model incompatible with route";
static const char *const MODEL_NOT_FOUND = "model not found";

static const char *const TRANSIENT[] = {"timeout", "overloaded", "server_error"};
static const char *const POLICY_REFUSAL[] = {
    "data_policy_denied", "content_policy_blocked", "guardrail_blocked"};
static const char *const PROVIDER_REFUSALS[] = {
    "timeout", "overloaded", "server_error", "auth", "auth_permanent", "billing",
    "rate_limit", "upstream_rate_limit"};
static const char *const REQUEST_SHAPE[] = {
    "format_error", "role_alternation", "multimodal_tool_content_unsupported",
    "reasoning_mandatory", "llama_cpp_grammar_pattern", "image_too_large", "image_corrupt",
    "invalid_encrypted_content", "thinking_signature", "context_overflow", "payload_too_large",
    "oauth_long_context_beta_forbidden"};

typedef struct { const char *reason, *label; } label_t;
static const label_t LABELS[] = {
    {"auth", AUTH_ERROR}, {"auth_permanent", AUTH_ERROR}, {"billing", PAYMENT_ERROR},
    {"rate_limit", "rate limit"}, {"upstream_rate_limit", "rate limit"},
    {"model_not_found", MODEL_NOT_FOUND},
    {"model_entitlement", MODEL_INCOMPATIBLE},
    {"incomplete_response", "invalid provider response"},
    {"overloaded", "provider overloaded"}, {"server_error", "server error"},
    {"long_context_tier", "long context tier"},
};
#define NARR(a) (sizeof(a) / sizeof((a)[0]))
#define NLAB (sizeof(LABELS) / sizeof(LABELS[0]))

static int in_set(const char *r, const char *const *set, size_t n) {
    if (!r) return 0;
    for (size_t i = 0; i < n; ++i) if (!strcmp(r, set[i])) return 1;
    return 0;
}
static const char *reason_of(const json_t *v) { return json_get_str(v, "reason", ""); }
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}
static const json_t *ctx_of(const json_t *v) {
    const json_t *c = json_obj_get(v, "error_context");
    return (c && c->type == JSON_OBJECT) ? c : NULL;
}
static int ctx_truthy(const json_t *v, const char *key) {
    const json_t *c = ctx_of(v);
    return c ? py_truthy(json_obj_get(c, key)) : 0;
}
static int ctx_has(const json_t *v, const char *key) {
    const json_t *c = ctx_of(v);
    return c && json_has(c, key);
}
static const char *ctx_str(const json_t *v, const char *key) {
    const json_t *c = ctx_of(v);
    return c ? json_get_str(c, key, "") : "";
}
static const char *label_for(const char *reason) {
    for (size_t i = 0; i < NLAB; ++i)
        if (!strcmp(reason, LABELS[i].reason)) return LABELS[i].label;
    return NULL;
}

int w15_auxiliarye_905344_same_provider_retry(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    return in_set(reason_of(v), TRANSIENT, NARR(TRANSIENT)) &&
           json_get_bool(v, "retryable", 1) ? 1 : 0;
}
int w15_auxiliarye_905344_auth_refresh(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    return !strcmp(reason_of(v), "auth") && json_get_bool(v, "should_rotate_credential", 0) ? 1 : 0;
}
int w15_auxiliarye_905344_same_key_retry(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    return !strcmp(reason_of(v), "rate_limit") && json_get_bool(v, "retryable", 1) &&
           !ctx_truthy(v, "quota_exhausted") ? 1 : 0;
}
int w15_auxiliarye_905344_resend_cannot_help(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    return in_set(reason_of(v), POLICY_REFUSAL, NARR(POLICY_REFUSAL)) ||
           ctx_truthy(v, "interrupted") ? 1 : 0;
}
int w15_auxiliarye_905344_benches_provider(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    return !strcmp(reason_of(v), "billing") || ctx_truthy(v, "quota_exhausted") ? 1 : 0;
}
int w15_auxiliarye_905344_stream_negotiation_reraises(const struct json_t *verdict) {
    const json_t *v = (const json_t *)verdict;
    const char *r = reason_of(v);
    return (in_set(r, PROVIDER_REFUSALS, NARR(PROVIDER_REFUSALS)) ||
            !strcmp(r, "ssl_cert_verification") ||
            json_get_num(v, "status_code", 0) == 429.0 ||
            w15_auxiliarye_905344_benches_provider(v) ||
            ctx_has(v, "welcome_refusal") ||
            w15_auxiliarye_905344_resend_cannot_help(v)) ? 1 : 0;
}

static json_t *jnull_or_string(const char *s) {
    if (!s) return json_null();
    return json_string(s);
}
static char *underscores_to_spaces(const char *s) {
    char *p = (char *)malloc(strlen(s ? s : "") + 1);
    if (!p) return NULL;
    char *q = p;
    for (const char *c = s ? s : ""; *c; ++c) *q++ = (*c == '_') ? ' ' : *c;
    *q = '\0';
    return p;
}

json_t *w15_auxiliarye_905344_fallback_label(const struct json_t *failure, int rung_spent) {
    const json_t *f = (const json_t *)failure;
    const json_t *v = json_obj_get(f, "verdict");
    const char *reason = reason_of(v);
    int retryable = json_get_bool(v, "retryable", 1);
    int should_fallback = json_get_bool(v, "should_fallback", 0);
    int rejects = json_get_bool(f, "rejects_parameter", 0);
    const char *structured = ctx_truthy(v, "statusless_structured")
                                 ? "structured provider error" : NULL;

    if (ctx_truthy(v, "hook_declines_fallback")) return json_null();
    if (w15_auxiliarye_905344_benches_provider(v)) return json_string(PAYMENT_ERROR);
    if (!strcmp(reason, "timeout")) {
        return json_string(!strcmp(ctx_str(v, "transport"), "connection")
                               ? "connection error" : "request timed out");
    }
    const char *lbl = label_for(reason);
    if (lbl) return json_string(lbl);
    if (!strcmp(reason, "content_policy_blocked") || !(retryable || should_fallback))
        return json_null();
    if (rejects || (in_set(reason, REQUEST_SHAPE, NARR(REQUEST_SHAPE)) &&
                    !ctx_truthy(v, "route_rejects_model"))) {
        if (rung_spent) return json_string(MODEL_INCOMPATIBLE);
        return jnull_or_string(rejects ? NULL : structured);
    }
    if (ctx_truthy(v, "route_rejects_model")) return json_string(MODEL_INCOMPATIBLE);
    if (should_fallback) {
        char *r = underscores_to_spaces(reason);
        json_t *out = json_string(r ? r : "");
        free(r);
        return out;
    }
    return jnull_or_string(structured);
}

int w15_auxiliarye_905344_ladder_accepts(const struct json_t *failure, int rung_spent) {
    const json_t *v = json_obj_get((const json_t *)failure, "verdict");
    if (json_get_bool(v, "should_rotate_credential", 0)) return 1;
    json_t *lbl = w15_auxiliarye_905344_fallback_label(failure, rung_spent);
    int yes = lbl && lbl->type != JSON_NULL;
    json_free(lbl);
    return yes ? 1 : 0;
}
