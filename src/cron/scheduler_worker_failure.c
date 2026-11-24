/*
 * scheduler_worker_failure.c — real C11 port of upstream Hermes
 * cron/scheduler_worker_failure.py (gate w17_schedulerw_4c800d).
 *
 * PoP: w17_schedulerw_4c800d_external_worker_exited_reason @ cron/scheduler_worker_failure.py:external_worker_exited_reason
 * PoP: w17_schedulerw_4c800d_record_outcome @ cron/scheduler_worker_failure.py:record_unknown_worker_outcome
 *
 * record_unknown_worker_outcome's cron.scheduler collaborators are injected as
 * explicit (exec_unknown / owns_claim) decisions; the C port reproduces the
 * early-return guards and the owner projection (including `or ""` coercion)
 * and returns the boolean result together with the fence's expected_owner.
 */
#include "noc_w17_schedulerw_4c800d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *py_str(const json_t *v) {
    char buf[96];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
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

static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return v->bool_val ? 0 : 1;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val || !v->str_val[0];
    if (v->type == JSON_ARRAY) return json_len(v) == 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) == 0;
    return 1;
}

char *w17_schedulerw_4c800d_external_worker_exited_reason(json_t *rc) {
    static const char PREFIX[] =
        "Restart-safe cron worker exited with status ";
    static const char SUFFIX[] =
        " after adopting this execution, without writing a durable terminal "
        "state; whether side effects ran is unknown.";
    char *r = py_str(rc);
    size_t cap;
    char *out;
    if (!r) return NULL;
    cap = strlen(PREFIX) + strlen(r) + strlen(SUFFIX) + 1;
    out = (char *)malloc(cap);
    if (out) {
        snprintf(out, cap, "%s%s%s", PREFIX, r, SUFFIX);
    }
    free(r);                       /* copy out before free */
    return out;
}

json_t *w17_schedulerw_4c800d_record_outcome(json_t *job, int exec_unknown, int owns_claim) {
    json_t *out = json_object();
    json_t *claim, *by;
    char *owner = NULL;
    (void)owns_claim;              /* result is True on every path past the owner guard */
    if (!exec_unknown) {
        json_set(out, "result", json_bool(false));
        json_set(out, "fence_owner", json_null());
        return out;
    }
    claim = job ? json_obj_get(job, "fire_claim") : NULL;
    by = (claim && claim->type == JSON_OBJECT) ? json_obj_get(claim, "by") : NULL;
    if (!is_falsy(by)) owner = py_str(by);
    if (!owner || owner[0] == '\0') {
        free(owner);
        json_set(out, "result", json_bool(false));
        json_set(out, "fence_owner", json_null());
        return out;
    }
    json_set(out, "result", json_bool(true));
    json_set(out, "fence_owner", json_string(owner));
    free(owner);
    return out;
}
