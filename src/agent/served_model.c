/*
 * served_model.c — faithful C11 port of upstream Hermes agent/served_model.py.
 *
 * PoP: servedmodel_from_headers @ agent/served_model.py:served_model_from_headers
 * PoP: servedmodel_result_fields @ agent/served_model.py:result_model_fields
 *
 * The hook-install routine (install_served_model_capture) mutates an httpx
 * client and is deliberately not part of the pure leaf port. The two pure
 * decision functions are ported exactly.
 */
#include "noc_w6_servedmodel_e72b96.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *const SM_HEADERS[] = {
    "x-litellm-model-id", "x-litellm-model-api-base"
};
#define SM_HEADER_COUNT 2

/* Python str.strip(): strip leading/trailing ASCII whitespace (in-place view). */
static const char *sm_trim(const char *s) {
    if (!s) return "";
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}
static size_t sm_trim_len(const char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    return n;
}

/* Python str(value) for the JSON scalar values we accept from a fixture. */
static char *sm_py_str(const json_t *v) {
    if (!v) return NULL;
    char buf[64];
    switch (v->type) {
        case JSON_STRING: {
            char *s = (char *)malloc(strlen(v->str_val) + 1);
            if (s) strcpy(s, v->str_val);
            return s;
        }
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%g", d);
            break;
        }
        case JSON_BOOL:
            snprintf(buf, sizeof(buf), "%s", v->bool_val ? "True" : "False");
            break;
        case JSON_NULL:
            snprintf(buf, sizeof(buf), "None");
            break;
        default:
            return NULL;
    }
    char *s = (char *)malloc(strlen(buf) + 1);
    if (s) strcpy(s, buf);
    return s;
}

static int sm_is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL && !v->bool_val) return 1;
    if (v->type == JSON_NUMBER && v->num_val == 0) return 1;
    if (v->type == JSON_STRING && v->str_val[0] == '\0') return 1;
    return 0;
}

json_t *servedmodel_from_headers(const json_t *headers) {
    if (!headers || headers->type != JSON_OBJECT) return json_null();
    for (size_t i = 0; i < SM_HEADER_COUNT; i++) {
        json_t *v = json_obj_get(headers, SM_HEADERS[i]);
        if (!v) continue;                      /* headers.get(name) -> None */
        /* Python: if value not in (None, ""): */
        if (v->type == JSON_NULL) continue;
        if (v->type == JSON_STRING && v->str_val[0] == '\0') continue;
        char *raw = sm_py_str(v);
        if (!raw) continue;
        const char *t = sm_trim(raw);
        size_t n = sm_trim_len(t);
        char *out = (char *)malloc(n + 1);
        if (out) { memcpy(out, t, n); out[n] = '\0'; }
        free(raw);
        json_t *res = out ? json_string(out) : json_null();
        free(out);
        return res;
    }
    return json_null();
}

json_t *servedmodel_result_fields(const json_t *last_served, const char *model,
                                  int fallback_activated, const json_t *primary_runtime) {
    /* served = agent.last_served_model ; requested = agent.model */
    json_t *served = (json_t *)last_served;    /* borrowed; copied into the result below */
    const char *requested = model ? model : "";

    if (sm_is_falsy(served) && fallback_activated) {
        const char *prim = "";
        if (primary_runtime && primary_runtime->type == JSON_OBJECT) {
            json_t *pm = json_obj_get(primary_runtime, "model");
            if (pm && pm->type == JSON_STRING) {
                const char *t = sm_trim(pm->str_val);
                size_t n = sm_trim_len(t);
                char *buf = (char *)malloc(n + 1);
                if (buf) { memcpy(buf, t, n); buf[n] = '\0'; prim = buf; }
            }
        }
        if (prim[0] && strcmp(prim, requested) != 0) {
            requested = prim;
            served = (json_t *)NULL;           /* placeholder; served becomes old `model` below */
            /* Python: requested, served = primary, agent.model */
            /* We must capture the OLD model as the served value. */
            json_t *old_model = json_string(model ? model : "");
            json_t *res = json_object();
            json_set(res, "requested_model", json_string(requested));
            json_set(res, "served_model", sm_is_falsy(old_model) ? json_null() : old_model);
            return res;
        }
    }

    json_t *res = json_object();
    json_set(res, "requested_model", json_string(requested));
    json_set(res, "served_model", sm_is_falsy(served) ? json_null() : json_copy(served));
    return res;
}
