/*
 * delegate_tool_registry.c - C home for upstream Hermes tools/delegate_tool_registry.py.
 * PoP: w16_delegateto_e7d732_attribution @ tools/delegate_tool_registry.py:get_subagent_attribution
 * PoP: w16_delegateto_e7d732_resume_spec @ tools/delegate_tool_registry.py:_resume_spec
 * PoP: w16_delegateto_e7d732_public_record @ tools/delegate_tool_registry.py:list_active_subagents
 *
 * Registry state is injected as JSON instead of read from process globals; the exported
 * functions are the pure projection/lookup parts of the live module. Locks, weakref walks and
 * the control plane stay in Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_e7d732.h"

static const json_t *obj_or_null(const json_t *o) { return (o && o->type == JSON_OBJECT) ? o : NULL; }
static json_t *value_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

struct json_t *w16_delegateto_e7d732_attribution(const struct json_t *state) {
    const char *task_id = json_get_str(state, "task_id", "");
    if (!task_id || !task_id[0]) return json_null();
    const json_t *active = obj_or_null(json_obj_get(state, "active"));
    const json_t *recent = obj_or_null(json_obj_get(state, "recent"));
    const json_t *rec = active ? json_obj_get(active, task_id) : NULL;
    if (!rec || rec->type == JSON_NULL || (rec->type == JSON_OBJECT && rec->c.count == 0))
        rec = recent ? json_obj_get(recent, task_id) : NULL;
    if (!rec || rec->type == JSON_NULL) return json_null();
    json_t *out = json_object();
    json_set(out, "subagent_id", json_string(task_id));
    json_set(out, "goal", value_or_null(json_obj_get(rec, "goal")));
    json_set(out, "delegation_id", value_or_null(json_obj_get(rec, "delegation_id")));
    return out;
}

struct json_t *w16_delegateto_e7d732_resume_spec(const struct json_t *record) {
    const json_t *child = obj_or_null(json_obj_get(record, "agent"));
    const json_t *build = child ? obj_or_null(json_obj_get(child, "_delegate_resume")) : NULL;
    const char *session_id = json_get_str(child, "session_id", "");
    if (!build || !json_has(build, "route") || !session_id || !session_id[0]) return json_null();
    json_t *out = json_copy(build);
    json_set(out, "session_id", json_string(session_id));
    json_set(out, "model", value_or_null(json_obj_get(record, "model")));
    return out;
}

struct json_t *w16_delegateto_e7d732_public_record(const struct json_t *record) {
    json_t *out = json_object();
    if (!record || record->type != JSON_OBJECT) return out;
    static const char *const PRIVATE[] = {
        "agent", "owner_session_id", "owner_transport", "owner_session_record", "accepting_steer"
    };
    for (size_t i = 0; i < json_object_size(record); i++) {
        const char *k = json_object_get_key_at(record, i);
        if (!k) continue;
        int skip = 0;
        for (size_t p = 0; p < 5; p++) if (!strcmp(k, PRIVATE[p])) { skip = 1; break; }
        if (skip) continue;
        json_set(out, k, json_copy(json_object_get_at(record, i)));
    }
    return out;
}

