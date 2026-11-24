/*
 * run_inbound_consumer.c — C port of upstream Hermes gateway/run_inbound_consumer.py.
 *
 * PoP: w16_runinbound_e075b2_hook_choice @ gateway/run_inbound_consumer.py:run_post_admission_hook
 *
 * The hook's payload/scope/transport work is I/O; the gated slice is the exact
 * fail-open decision over the plugin results, with those results injected as a seam.
 */
#include "noc_w16_runinbound_e075b2.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *WS = " \t\n\r\f\v";
static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(WS, s[a])) a++;
    while (b > a && strchr(WS, s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) { memcpy(p, s + a, b - a); p[b - a] = '\0'; }
    return p;
}

json_t *w16_runinbound_e075b2_hook_choice(const json_t *results, const json_t *mode) {
    json_t *out = json_object();
    /* Upstream fail-open paths: no hook, or the hook raised -> (False, None). */
    if (json_is_string(mode) && mode->str_val &&
        (!strcmp(mode->str_val, "has_hook_false") || !strcmp(mode->str_val, "hook_error"))) {
        json_set(out, "handled", json_bool(false));
        json_set(out, "reply", json_null());
        return out;
    }
    if (json_is_array(results)) {
        for (size_t i = 0; i < json_len(results); i++) {
            const json_t *res = json_get(results, i);
            if (!json_is_object(res)) continue;
            const json_t *act = json_obj_get(res, "action");
            if (!json_is_string(act) || !act->str_val || strcmp(act->str_val, "handled")) continue;
            json_set(out, "handled", json_bool(true));
            const json_t *reply = json_obj_get(res, "reply");
            if (json_is_string(reply)) {
                char *trimmed = py_strip(reply->str_val ? reply->str_val : "");
                int keep = trimmed && trimmed[0];
                free(trimmed);
                json_set(out, "reply", keep ? json_copy(reply) : json_null());
            } else {
                json_set(out, "reply", json_null());
            }
            return out;
        }
    }
    json_set(out, "handled", json_bool(false));
    json_set(out, "reply", json_null());
    return out;
}
