/*
 * route_verdicts.c — C11 port of the pure per-route verdict core of upstream
 * Hermes agent/route_verdicts.py.
 *
 * PoP: w7_routeverdic_40e4a2_route_visit @ agent/route_verdicts.py:route_visit
 * PoP: w7_routeverdic_40e4a2_snapshot @ agent/route_verdicts.py:snapshot_route_verdicts
 * PoP: w7_routeverdic_40e4a2_apply @ agent/route_verdicts.py:apply_route_verdicts
 * PoP: w7_routeverdic_40e4a2_reset @ agent/route_verdicts.py:reset_route_verdicts
 * PoP: w7_routeverdic_40e4a2_restore_primary @ agent/route_verdicts.py:restore_primary_route_verdicts
 *
 * normalize_route_base_url and reset_codex_reasoning_replay are external: the
 * former is supplied as an already-normalized endpoint seam, the latter's two
 * field assignments are inlined (enabled=True, rejected=False).
 */
#include "noc_w7_routeverdic_40e4a2.h"
#include <math.h>

static const char *const VERDICT_FIELDS[] = {
    "_reasoning_disable_rejected", "_reasoning_floor_required",
    "_reasoning_effort_rejected", "_stream_options_unsupported",
    "_disable_streaming", "_codex_reasoning_replay_enabled",
    "_codex_reasoning_replay_rejected", NULL
};
static const char *const REJECTION_FIELDS[] = {
    "_reasoning_disable_rejected", "_reasoning_floor_required",
    "_reasoning_effort_rejected", "_stream_options_unsupported", NULL
};

long long w7_routeverdic_40e4a2_route_visit(const json_t *visit) {
    if (!visit) return 0;
    if (visit->type == JSON_BOOL) return visit->bool_val ? 1 : 0;
    if (visit->type == JSON_NUMBER) {
        double d = visit->num_val;
        if (isfinite(d) && d == floor(d) && d >= -9.22e18 && d <= 9.22e18)
            return (long long)d;
    }
    return 0; /* string / null / array / object / non-integral float */
}

json_t *w7_routeverdic_40e4a2_snapshot(const json_t *agent) {
    json_t *out = json_object();
    if (!json_is_object(agent)) return out;
    for (int i = 0; VERDICT_FIELDS[i]; i++) {
        json_t *v = json_obj_get(agent, VERDICT_FIELDS[i]);
        if (v) json_set(out, VERDICT_FIELDS[i], json_copy(v));
    }
    return out;
}

json_t *w7_routeverdic_40e4a2_apply(const json_t *agent, const json_t *values) {
    json_t *out = json_is_object(agent) ? json_copy(agent) : json_object();
    if (json_is_object(values)) {
        for (size_t i = 0; i < json_object_size(values); i++) {
            const char *k = json_object_get_key_at(values, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(values, i)));
        }
    }
    return out;
}

json_t *w7_routeverdic_40e4a2_reset(const json_t *agent, bool streaming_seed) {
    json_t *out = json_is_object(agent) ? json_copy(agent) : json_object();
    long long visit = w7_routeverdic_40e4a2_route_visit(
        json_is_object(agent) ? json_obj_get(agent, "_route_visit") : NULL) + 1;
    json_set(out, "_route_visit", json_number((double)visit));
    for (int i = 0; REJECTION_FIELDS[i]; i++)
        json_set(out, REJECTION_FIELDS[i], json_bool(false));
    if (json_is_object(agent) && json_has(agent, "_disable_streaming"))
        json_set(out, "_disable_streaming", json_bool(streaming_seed));
    /* reset_codex_reasoning_replay(agent) */
    json_set(out, "_codex_reasoning_replay_enabled", json_bool(true));
    json_set(out, "_codex_reasoning_replay_rejected", json_bool(false));
    return out;
}

json_t *w7_routeverdic_40e4a2_restore_primary(const json_t *rt, const char *endpoint,
                                              const json_t *agent) {
    json_t *rt_out = json_is_object(rt) ? json_copy(rt) : json_object();
    json_t *agent_out = json_is_object(agent) ? json_copy(agent) : json_object();
    json_t *saved = json_obj_get(rt_out, "route_verdicts");
    if (json_is_object(saved)) {
        const char *base = json_get_str(saved, "base_url", "");
        if (endpoint && base && !strcmp(base, endpoint)) {
            json_t *values = json_obj_get(saved, "values");
            if (json_is_object(values) && json_object_size(values) > 0) {
                json_t *applied = w7_routeverdic_40e4a2_apply(agent_out, values);
                json_free(agent_out);
                agent_out = applied;
            }
        }
    }
    json_obj_del(rt_out, "route_verdicts");
    json_t *result = json_array();
    json_append(result, rt_out);
    json_append(result, agent_out);
    return result;
}

