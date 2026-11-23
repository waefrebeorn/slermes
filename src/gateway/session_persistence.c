/*
 * session_persistence.c — real C11 port of upstream Hermes gateway/session_persistence.py.
 * PoP: w14_sessionper_16f973_chat_labels_meta_key @ gateway/session_persistence.py:_chat_labels_meta_key
 * PoP: w14_sessionper_16f973_is_live_system_guard @ gateway/session_persistence.py:_is_live_system_guard
 * Ported: the module-level deterministic helpers. The SessionStore mixin's
 * database/filesystem plumbing is seam-scoped out (it is not a pure decision
 * core and cannot be gated against a live authority).
 */
#include "hermes_json.h"
#include "noc_w14_sessionper_16f973.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return v->c.count ? 1 : 0;
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

/* Python str()/f-string form of a JSON scalar as the fixtures supply it. */
static void scalar_str(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    switch (v->type) {
        case JSON_STRING:
            snprintf(out, cap, "%s", v->str_val ? v->str_val : "");
            return;
        case JSON_BOOL:
            snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
            return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val &&
                v->num_val >= -9007199254740992.0 && v->num_val <= 9007199254740992.0)
                snprintf(out, cap, "%lld", (long long)v->num_val);
            else
                snprintf(out, cap, "%.17g", v->num_val);
            return;
        default:
            snprintf(out, cap, "");
            return;
    }
}

char *w14_sessionper_16f973_chat_labels_meta_key(const struct json_t *platform,
                                                 const struct json_t *scope_id,
                                                 const struct json_t *chat_id) {
    char pv[1024], sv[1024], cv[1024], buf[3200];
    const char *scope = "";
    scalar_str(platform, pv, sizeof(pv));
    scalar_str(chat_id, cv, sizeof(cv));
    if (py_truthy(scope_id)) {
        scalar_str(scope_id, sv, sizeof(sv));
        scope = sv;
    }
    snprintf(buf, sizeof(buf), "gateway_chat_labels:%s:%s:%s", pv, scope, cv);
    {
        size_t n = strlen(buf) + 1;
        char *out = (char *)malloc(n);
        if (out) memcpy(out, buf, n);
        return out;
    }
}

int w14_sessionper_16f973_is_live_system_guard(const char *exc_type,
                                               const char *exc_message) {
    if (!exc_type || strcmp(exc_type, "RuntimeError")) return 0;
    return exc_message && strstr(exc_message, "live-system guard") ? 1 : 0;
}
