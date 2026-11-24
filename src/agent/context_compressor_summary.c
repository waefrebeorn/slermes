/*
 * context_compressor_summary.c -- real C11 port of the summary-hook dispatch
 * helpers of upstream Hermes agent/context_compressor_summary.py: the keyword
 * probe over an inspectable callable and the summary-route pinning that decides
 * which model a compression call names after a fallback.
 *
 * PoP: w20_contextcom_4ebd6d_accepts_keyword_argument @ agent/context_compressor_summary.py:accepts_keyword_argument
 * PoP: w20_contextcom_4ebd6d_apply_summary_route @ agent/context_compressor_summary.py:SummaryDispatchMixin._apply_summary_route
 */
#include "hermes_json.h"
#include "noc_w20_contextcom_4ebd6d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ctx4_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ctx4_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static void ctx4_number_text(char *dst, size_t cap, double d) {
    int precision;
    if (isfinite(d) && floor(d) == d && fabs(d) <= 9007199254740992.0) {
        snprintf(dst, cap, "%lld", (long long)d);
        return;
    }
    dst[0] = '\0';
    for (precision = 1; precision <= 17; precision++) {
        snprintf(dst, cap, "%.*g", precision, d);
        if (strtod(dst, NULL) == d) break;
    }
    if (!strpbrk(dst, ".eEnN")) {
        size_t n = strlen(dst);
        if (n + 3 <= cap) { dst[n] = '.'; dst[n + 1] = '0'; dst[n + 2] = '\0'; }
    }
}

/* str(value) for the scalars a route field can carry. */
static char *ctx4_str_of(const json_t *v) {
    char buf[48];
    if (!v) return ctx4_dup("");
    switch (v->type) {
    case JSON_STRING: return ctx4_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return ctx4_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        ctx4_number_text(buf, sizeof(buf), v->num_val);
        return ctx4_dup(buf);
    case JSON_NULL:   return ctx4_dup("None");
    default:          return ctx4_dup("");
    }
}

/* inspect.Parameter.KIND ordering as the spec spells it. */
enum {
    CTX4_KIND_POS_ONLY = 0,
    CTX4_KIND_POS_OR_KW,
    CTX4_KIND_VAR_POS,
    CTX4_KIND_KW_ONLY,
    CTX4_KIND_VAR_KW
};

static int ctx4_kind_of(const char *kind) {
    if (!kind) return CTX4_KIND_POS_OR_KW;
    if (!strcmp(kind, "pos_only")) return CTX4_KIND_POS_ONLY;
    if (!strcmp(kind, "pos_or_kw")) return CTX4_KIND_POS_OR_KW;
    if (!strcmp(kind, "var_pos")) return CTX4_KIND_VAR_POS;
    if (!strcmp(kind, "kw_only")) return CTX4_KIND_KW_ONLY;
    if (!strcmp(kind, "var_kw")) return CTX4_KIND_VAR_KW;
    return CTX4_KIND_POS_OR_KW;
}

struct json_t *w20_contextcom_4ebd6d_accepts_keyword_argument(const struct json_t *signature_spec,
                                                             const char *name) {
    json_t *params;
    size_t i;
    int found_other = 0;

    if (!signature_spec || signature_spec->type != JSON_OBJECT) return json_bool(0);
    params = json_obj_get(signature_spec, "params");
    if (params && params->type == JSON_ARRAY) {
        for (i = 0; i < params->c.count; i++) {
            json_t *param = params->c.items[i];
            const char *pname, *pkind;
            int kind;
            if (!param || param->type != JSON_OBJECT) continue;
            pname = json_get_str(param, "name", "");
            pkind = json_get_str(param, "kind", "pos_or_kw");
            kind = ctx4_kind_of(pkind);
            if (kind == CTX4_KIND_VAR_KW) return json_bool(1);   /* VAR_KEYWORD wins */
            if (name && !strcmp(pname, name)) {
                found_other = (kind == CTX4_KIND_POS_OR_KW || kind == CTX4_KIND_KW_ONLY) ? 1 : 2;
            }
        }
        if (found_other == 2) return json_bool(0);
        if (found_other == 1) return json_bool(1);
    }
    if (json_get_bool(signature_spec, "var_keyword", 0)) return json_bool(1);
    return json_bool(0);
}

static json_t *ctx4_copy_without(const json_t *src, const char *const *drop, size_t ndrop) {
    json_t *out = json_object();
    size_t i, j;
    if (!src || src->type != JSON_OBJECT) return out;
    for (i = 0; i < src->c.count; i++) {
        const char *key = src->c.keys[i];
        int skip = 0;
        if (!key) continue;
        for (j = 0; j < ndrop; j++) if (!strcmp(key, drop[j])) skip = 1;
        if (skip) continue;
        json_set(out, key, json_copy(src->c.items[i]));
    }
    return out;
}

struct json_t *w20_contextcom_4ebd6d_apply_summary_route(const struct json_t *call_kwargs,
                                                         const struct json_t *pinned,
                                                         const struct json_t *self_spec) {
    static const char *const ROUTE_KEYS[] = {"provider", "model", "base_url", "api_key", "api_mode"};
    static const char *const ROUTE_ORDER[] = {"provider", "model", "base_url", "api_key", "api_mode"};
    json_t *out;
    size_t i;

    if (ctx4_truthy(pinned) && pinned->type == JSON_OBJECT) {
        out = ctx4_copy_without(call_kwargs, ROUTE_KEYS, 5);
        for (i = 0; i < pinned->c.count; i++) {
            const char *key = pinned->c.keys[i];
            if (!key) continue;
            json_set(out, key, json_copy(pinned->c.items[i]));
        }
        return out;
    }
    out = ctx4_copy_without(call_kwargs, NULL, 0);
    if (self_spec && self_spec->type == JSON_OBJECT) {
        json_t *summary_model = json_obj_get(self_spec, "summary_model");
        if (ctx4_truthy(summary_model)) {
            json_set(out, "model", json_copy(summary_model));
            return out;
        }
        if (!json_get_bool(self_spec, "_summary_model_fallen_back", 0)) return out;
    } else {
        return out;
    }
    {
        json_t *task = json_obj_get(out, "task");
        char *task_text = ctx4_truthy(task) ? ctx4_str_of(task) : ctx4_dup("compression");
        json_set(out, "task", json_string(task_text ? task_text : ""));
        free(task_text);
    }
    for (i = 0; i < 5; i++) {
        json_t *value = json_obj_get(self_spec, ROUTE_ORDER[i]);
        if (ctx4_truthy(value)) json_set(out, ROUTE_ORDER[i], json_copy(value));
    }
    return out;
}
