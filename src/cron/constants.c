/*
 * constants.c — C port of upstream Hermes cron/constants.py.
 * PoP: cronconst_is_recurring @ cron/constants.py:is_recurring
 * PoP: cronconst_fire_claim_ttl @ cron/constants.py:FIRE_CLAIM_TTL_SECONDS
 * PoP: cronconst_fire_claim_skew @ cron/constants.py:FIRE_CLAIM_SKEW_SECONDS
 * PoP: cronconst_inactivity_headroom @ cron/constants.py:CLAIM_TTL_INACTIVITY_HEADROOM
 */
#include "noc_cronconst.h"
#include "hermes_json.h"
#include <string.h>

static int falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) return !v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;
    if (v->type == JSON_STRING) return !v->str_val || !v->str_val[0];
    if (v->type == JSON_ARRAY) return json_len(v) == 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) == 0;
    return 0;
}
int cronconst_is_recurring(const json_t *job) {
    if (!job || job->type != JSON_OBJECT) return 0;
    const json_t *sched = json_obj_get(job, "schedule");
    if (!sched || falsy(sched)) return 0;
    if (sched->type != JSON_OBJECT) return 0;
    const json_t *kind = json_obj_get(sched, "kind");
    const char *k = kind && kind->type == JSON_STRING ? kind->str_val : NULL;
    if (!k) return 0;
    return (!strcmp(k, "cron") || !strcmp(k, "interval")) ? 1 : 0;
}
