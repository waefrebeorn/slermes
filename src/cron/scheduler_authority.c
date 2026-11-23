/*
 * scheduler_authority.c — C home for upstream Hermes cron/scheduler_authority.py.
 *
 * PoP: w10_schedulera_538417_pause_identity @ cron/scheduler_authority.py:_pause_identity
 * PoP: w10_schedulera_538417_replayable @ cron/scheduler_authority.py:replayable_completions
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w10_schedulera_538417.h"

/* Mirrors: {'paused_at': paused.get('paused_at'),
 * 'reason': paused.get('paused_reason')} if paused else True.
 * A JSON object carrying keys is truthy; {} / null / false / 0 / "" / [] are falsy. */
json_t *w10_schedulera_538417_pause_identity(const json_t *in) {
    const json_t *paused = json_obj_get(in, "paused");
    if (paused && paused->type == JSON_OBJECT && json_object_size(paused) > 0) {
        const json_t *at = json_obj_get(paused, "paused_at");
        const json_t *reason = json_obj_get(paused, "paused_reason");
        json_t *out = json_object();
        json_set(out, "paused_at", at ? json_copy(at) : json_null());
        json_set(out, "reason", reason ? json_copy(reason) : json_null());
        return out;
    }
    return json_bool(1);
}

/* Mirrors: [eid for eid in completions or () if journal_path(job_id, eid).exists()].
 * The fixture injects the journal-backed id set through "existing", so the C port
 * applies exactly the same predicate (order and duplicates preserved). */
json_t *w10_schedulera_538417_replayable(const json_t *in) {
    const json_t *comps = json_obj_get(in, "completions");
    const json_t *existing = json_obj_get(in, "existing");
    json_t *out = json_array();
    if (comps && comps->type == JSON_ARRAY) {
        size_t n = json_len(comps);
        for (size_t i = 0; i < n; i++) {
            const json_t *eid = json_get(comps, i);
            if (!eid || eid->type != JSON_STRING || !eid->str_val) continue;
            int found = 0;
            if (existing && existing->type == JSON_ARRAY) {
                size_t m = json_len(existing);
                for (size_t j = 0; j < m; j++) {
                    const json_t *e = json_get(existing, j);
                    if (e && e->type == JSON_STRING && e->str_val &&
                        !strcmp(e->str_val, eid->str_val)) { found = 1; break; }
                }
            }
            if (found) json_append(out, json_string(eid->str_val));
        }
    }
    return out;
}
