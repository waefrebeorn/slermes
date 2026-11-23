/*
 * job_definition.c — C home for upstream Hermes cron/job_definition.py.
 * Ported behavior: the authored-merge policy. Import-time dependencies
 * (normalize_timezone, parse_schedule, is_job_runnable, _apply_schedule_update,
 * _clear_quota_hold, prune_inherited_knobs) are injected as deterministic seams
 * by the oracle; the store I/O wrapper stays in Python.
 * PoP: w6_jobdefiniti_merge_job_definition @ cron/job_definition.py:merge_job_definition
 */
#include "noc_w6_jobdefiniti_2ab816.h"
#include <string.h>

/* JOB_DEFINITION_FIELDS (frozenset) — iteration order is irrelevant: every
 * update is a plain dict write, so a fixed order yields the same final dict. */
static const char *const DEFINITION_FIELDS[] = {
    "name", "prompt", "skills", "skill", "model", "provider", "base_url",
    "script", "no_agent", "monitor_script", "monitor_url", "context_from",
    "schedule", "schedule_display", "deliver", "origin", "enabled_toolsets",
    "workdir", "attach_to_session", "reasoning_effort", "failure_deliver",
    "timezone", "grace_minutes", "catch_up", "wrap_response"
};
#define N_DEFINITION_FIELDS (sizeof(DEFINITION_FIELDS) / sizeof(DEFINITION_FIELDS[0]))

static int is_definition_field(const char *k) {
    size_t i;
    if (!k) return 0;
    for (i = 0; i < N_DEFINITION_FIELDS; i++)
        if (!strcmp(k, DEFINITION_FIELDS[i])) return 1;
    return 0;
}

/* dict.get(k) where a JSON null is treated the same as a missing key. */
static json_t *get_or_null(const json_t *o, const char *k) {
    json_t *v = json_obj_get((json_t *)o, k);
    return (v && v->type != JSON_NULL) ? v : NULL;
}

/* Python `==` for the scalar values used as schedule / timezone. */
static int json_equal(const json_t *a, const json_t *b) {
    char *sa = a ? json_dumps(a, 0) : NULL;
    char *sb = b ? json_dumps(b, 0) : NULL;
    int eq;
    if (!sa && !sb) eq = 1;
    else if (sa && sb) eq = (strcmp(sa, sb) == 0);
    else eq = 0;
    free(sa);
    free(sb);
    return eq;
}

/* Python truthiness of a JSON value. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

json_t *w6_jobdefiniti_merge_job_definition(
    const json_t *local, const json_t *authored)
{
    json_t *merged = json_object();
    size_t i;

    /* {key: value for key, value in local.items()
     *  if key not in JOB_DEFINITION_FIELDS} */
    for (i = 0; i < json_object_size(local); i++) {
        const char *k = json_object_get_key_at(local, i);
        if (!k || is_definition_field(k)) continue;
        json_set(merged, k, json_copy(json_object_get_at(local, i)));
    }

    /* merged.update(authored definition fields) */
    for (i = 0; i < N_DEFINITION_FIELDS; i++) {
        json_t *v = json_obj_get((json_t *)authored, DEFINITION_FIELDS[i]);
        if (v) json_set(merged, DEFINITION_FIELDS[i], json_copy(v));
    }

    /* merged["repeat"] = {"completed": local.repeat.completed or 0,
     *                     "times": authored.repeat.times or None} */
    {
        json_t *rep = json_object();
        json_t *lr = get_or_null(local, "repeat");
        json_t *ar = get_or_null(authored, "repeat");
        long long completed = 0;
        json_t *times = NULL;
        if (lr && lr->type == JSON_OBJECT) {
            json_t *c = get_or_null(lr, "completed");
            if (c && c->type == JSON_NUMBER) completed = (long long)c->num_val;
        }
        if (ar && ar->type == JSON_OBJECT) {
            json_t *t = json_obj_get((json_t *)ar, "times");
            if (t && t->type != JSON_NULL) times = json_copy(t);
        }
        json_set(rep, "completed", json_number((double)completed));
        json_set(rep, "times", times ? times : json_null());
        json_set(merged, "repeat", rep);
    }

    /* timezone: normalize_timezone is identity under the seam, and
     * prune_inherited_knobs is a no-op. schedule: parse_schedule is identity
     * for strings under the seam. Both leave the value unchanged. */

    if (!json_equal(get_or_null(local, "schedule"), get_or_null(merged, "schedule")) ||
        !json_equal(get_or_null(local, "timezone"), get_or_null(merged, "timezone"))) {
        json_t *ms = get_or_null(merged, "schedule");
        json_obj_del(merged, "pending_slot"); /* merged.pop("pending_slot", None) */
        /* _clear_quota_hold(merged) is a no-op under the seam. */
        if (py_truthy(ms)) { /* is_job_runnable: bool(merged.get("schedule")) */
            json_t *updates = json_object();
            json_t *sd;
            size_t j;
            json_set(updates, "schedule", json_copy(ms));
            sd = json_obj_get((json_t *)authored, "schedule_display");
            if (sd) json_set(updates, "schedule_display", json_copy(sd));
            /* _apply_schedule_update: merged.update(updates) */
            for (j = 0; j < json_object_size(updates); j++) {
                const char *k = json_object_get_key_at(updates, j);
                if (k) json_set(merged, k, json_copy(json_object_get_at(updates, j)));
            }
            json_free(updates);
        } else {
            json_set(merged, "next_run_at", json_null());
        }
    }
    return merged;
}
