/*
 * execution_identity.c - C home for upstream Hermes cron/execution_identity.py.
 *
 * PoP: w9_executionid_8a3a66_enter_cron_execution_ex @ cron/execution_identity.py:enter_cron_execution
 * PoP: w9_executionid_8a3a66_current_cron_execution_ex @ cron/execution_identity.py:current_cron_execution
 *
 * ContextVar/Token bookkeeping is interpreter-local; the deterministic, portable
 * part of the module (building the immutable CronExecution identity from a job +
 * ledger row, and the delegated-child suppression) is exposed through explicit
 * seams.  The oracle drives the LIVE functions with stubbed ContextVar/profile/
 * delegation collaborators so exactly this logic is compared.
 */
#include "noc_w9_executionid_8a3a66.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int w9ei_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:  return 0;
        case JSON_BOOL:  return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        default: return v->c.count ? 1 : 0;
    }
}

/* str() coercion for the ASCII scalar domain used by the fixtures. */
static char *w9ei_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return _strdup("None");
    if (v->type == JSON_STRING) return _strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return _strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return _strdup(buf);
    }
    return _strdup("");
}

static json_t *w9ei_copy_or_null(const json_t *v) {
    return (v && v->type != JSON_NULL) ? json_copy(v) : json_null();
}

json_t *w9_executionid_8a3a66_enter_cron_execution_ex(json_t *job, const char *execution_id,
                                                      json_t *record, const char *profile) {
    if (!job) job = json_null();
    if (!record) record = json_null();

    json_t *idv = json_obj_get(job, "id");
    json_t *namev = json_obj_get(job, "name");
    char *job_id = w9ei_str(idv);
    char *job_name = w9ei_truthy(namev) ? w9ei_str(namev) : w9ei_str(idv);

    json_t *srcv = json_obj_get(record, "source");
    char *source = w9ei_truthy(srcv) ? w9ei_str(srcv) : _strdup("");

    const char *prof = (profile && profile[0]) ? profile : "default";

    json_t *out = json_object();
    json_set(out, "job_id", json_string(job_id));
    json_set(out, "job_name", json_string(job_name));
    json_set(out, "execution_id", json_string(execution_id ? execution_id : ""));
    json_set(out, "source", json_string(source));
    json_set(out, "scheduled_instant", w9ei_copy_or_null(json_obj_get(record, "scheduled_instant")));
    json_set(out, "started_at", w9ei_copy_or_null(json_obj_get(record, "started_at")));
    json_set(out, "profile", json_string(prof));

    free(job_id);
    free(job_name);
    free(source);
    return out;
}

json_t *w9_executionid_8a3a66_current_cron_execution_ex(json_t *bound, int delegated) {
    if (delegated || !bound || bound->type == JSON_NULL) return json_null();
    return json_copy(bound);
}
