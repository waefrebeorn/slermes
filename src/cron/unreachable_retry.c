/*
 * unreachable_retry.c — C port of upstream Hermes cron/unreachable_retry.py.
 *
 * PoP: unreachable_retry_enabled @ cron/unreachable_retry.py:retry_enabled
 * PoP: unreachable_is_retry_fire @ cron/unreachable_retry.py:is_retry_fire
 * PoP: unreachable_clear_state @ cron/unreachable_retry.py:clear_state
 * PoP: unreachable_ladder_applies @ cron/unreachable_retry.py:_ladder_applies
 *
 * The clock/datetime and _seconds_after helpers used by the ladder planner are
 * not reachable from these pure predicates; the config and is_recurring late
 * imports are explicit fixture seams (see the matching oracle).
 */
#include "noc_w8_unreachable_01eb9c.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

#define STATE_KEY "unreachable_retry"

static int retry_enabled_bool(json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return 1; /* profile config unreadable -> ON */
    json_t *cron_cfg = json_obj_get(cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return 1;
    json_t *v = json_obj_get(cron_cfg, "retry_unreachable");
    /* `is not False`: only a literal JSON false disables it. */
    if (v && v->type == JSON_BOOL && !v->bool_val) return 0;
    return 1;
}
static int eq_opt_str(const char *a, const char *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return !strcmp(a, b);
}
static const char *opt_str(json_t *v) {
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

json_t *unreachable_retry_enabled(json_t *cfg) {
    return json_bool(retry_enabled_bool(cfg));
}

json_t *unreachable_is_retry_fire(json_t *job, const char *next_run) {
    json_t *state = job ? json_obj_get(job, STATE_KEY) : NULL;
    const char *at = NULL, *expr = NULL;
    if (state && state->type == JSON_OBJECT) {
        at = opt_str(json_obj_get(state, "at"));
        expr = opt_str(json_obj_get(state, "expr"));
    }
    const char *sched_expr = NULL;
    json_t *sched = job ? json_obj_get(job, "schedule") : NULL;
    if (sched && sched->type == JSON_OBJECT)
        sched_expr = opt_str(json_obj_get(sched, "expr"));
    return json_bool(eq_opt_str(at, next_run) && eq_opt_str(expr, sched_expr));
}

json_t *unreachable_clear_state(json_t *job) {
    json_t *out = job ? json_copy(job) : json_object();
    if (out && out->type == JSON_OBJECT) json_obj_del(out, STATE_KEY);
    return out;
}

json_t *unreachable_ladder_applies(json_t *job, int recurring, json_t *cfg) {
    if (!recurring) return json_bool(false);
    const char *state = job ? json_get_str(job, "state", "") : "";
    if (state && !strcmp(state, "paused")) return json_bool(false);
    return json_bool(retry_enabled_bool(cfg));
}
