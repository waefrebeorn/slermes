/*
 * run_inbound_moa.c — C home for upstream Hermes gateway/run_inbound_moa.py.
 *
 * PoP: w9_runinbound_bcb6b7_moa_decision @ gateway/run_inbound_moa.py:_hm_cmd_moa
 *
 * /moa is one-shot sugar: run a single prompt through the default MoA preset,
 * then restore the prior model.  The upstream method is an async mixin whose
 * only logic is this decision tree over injected collaborators; the C port
 * mirrors every branch (including the early empty-payload return and the
 * local-vs-non-local policy split) exactly.
 */
#include "noc_w9_runinbound_bcb6b7.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python str.strip() for the ASCII whitespace the command payload uses. */
static char *py_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = '\0'; }
    return r;
}

static json_t *text_or_null(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return v ? json_copy(v) : json_null();
}

struct json_t *w9_runinbound_bcb6b7_moa_decision(const struct json_t *in) {
    json_t *out = json_object();
    char *payload = py_strip(json_get_str(in, "command_args", ""));

    int policy_present   = json_is_true(json_obj_get(in, "policy_present")) ? 1 : 0;
    int active_sink      = json_is_true(json_obj_get(in, "active_sink_present")) ? 1 : 0;
    int managed          = json_is_true(json_obj_get(in, "managed")) ? 1 : 0;
    int rebind_ok        = json_is_true(json_obj_get(in, "rebind_ok")) ? 1 : 0;
    int admit_raises     = json_is_true(json_obj_get(in, "admit_raises")) ? 1 : 0;

    /* moa_payload = event.get_command_args().strip(); if not moa_payload: ... */
    if (!payload || payload[0] == '\0') {
        json_set(out, "handled", json_bool(true));
        json_set(out, "reply", text_or_null(in, "usage_text"));   /* moa_usage() */
        json_set(out, "event_text", json_null());
        free(payload);
        return out;
    }

    if (policy_present) {
        /* policy = policy_for_source(...); if policy is not None: */
        if (active_sink) {
            /* active_sink() is not None */
            if (managed) {
                /* policy_if_managed(policy) is not None -> managed-session notice */
                json_set(out, "handled", json_bool(true));
                json_set(out, "reply", text_or_null(in, "managed_text"));
                json_set(out, "event_text", json_null());
            } else {
                /* slash.exec: client submits /moa as typed, managed worker never binds it */
                json_set(out, "handled", json_bool(false));
                json_set(out, "reply", json_null());
                json_set(out, "event_text", json_null());
            }
        } else if (!rebind_ok) {
            /* rebind_model_override(...) failed -> prepare-failed notice */
            json_set(out, "handled", json_bool(true));
            json_set(out, "reply", text_or_null(in, "prepare_failed_text"));
            json_set(out, "event_text", json_null());
        } else {
            /* event.text = moa_payload; return False, None */
            json_set(out, "handled", json_bool(false));
            json_set(out, "reply", json_null());
            json_set(out, "event_text", json_string(payload));
        }
    } else {
        /* policy is None: event.text = moa_payload first, then admit/claim.
         * Any prepare exception (admit_raises) yields the prepare-failed notice,
         * but event.text was already assigned. */
        json_set(out, "handled", json_bool(admit_raises ? true : false));
        json_set(out, "reply", admit_raises ? text_or_null(in, "prepare_failed_text") : json_null());
        json_set(out, "event_text", json_string(payload));
    }

    free(payload);
    return out;
}
