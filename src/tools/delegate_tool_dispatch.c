/*
 * delegate_tool_dispatch.c - C home for upstream Hermes tools/delegate_tool_dispatch.py.
 * PoP: w16_delegateto_2d04d4_resolve_wake @ tools/delegate_tool_dispatch.py:_resolve_async_wake_sid
 * PoP: w16_delegateto_2d04d4_batch_progress @ tools/delegate_tool_dispatch.py:_batch_progress_token
 * PoP: w16_delegateto_2d04d4_sync_note @ tools/delegate_tool_dispatch.py:_SYNC_FALLBACK_NOTES
 *
 * Session facts (finite mode, env flags, async support) and child summaries are injected via
 * explicit seams instead of process globals. Pool/runner orchestration stays in Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_2d04d4.h"

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

struct json_t *w16_delegateto_2d04d4_resolve_wake(const struct json_t *opts) {
    int oneshot = json_get_bool(opts, "oneshot_session", false);
    const json_t *finite = json_obj_get(opts, "finite");
    int finite_null = !finite || finite->type == JSON_NULL;
    int finite_true = finite && finite->type == JSON_BOOL && finite->bool_val;
    const char *single = json_get_str(opts, "single_query", "");
    int async_supported = json_get_bool(opts, "async_supported", false);
    int async_raises = json_get_bool(opts, "async_raises", false);
    const char *wake = json_get_str(opts, "wake_sid", "");
    int hist = json_get_bool(opts, "history_delivery", false);
    if (oneshot || finite_true || (finite_null && single && !strcmp(single, "1"))) return json_null();
    if (async_raises || async_supported) return json_string("");
    if (wake && wake[0] && hist) return json_string(wake);
    return json_null();
}

struct json_t *w16_delegateto_2d04d4_batch_progress(const struct json_t *child_agents) {
    json_t *out = json_object();
    json_t *parts = json_array();
    int in_tool = 0;
    if (child_agents && child_agents->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(child_agents); i++) {
            const json_t *c = json_get(child_agents, i);
            if (!c || c->type == JSON_NULL || c->type != JSON_OBJECT || json_get_bool(c, "raises", false)) {
                json_append(parts, json_null());
                continue;
            }
            double count = json_get_num(c, "api_call_count", 0);
            const json_t *tool = json_obj_get(c, "current_tool");
            const json_t *ts = json_obj_get(c, "last_activity_ts");
            if (truthy(tool)) in_tool = 1;
            json_t *row = json_array();
            json_append(row, json_number(count));
            json_append(row, tool ? json_copy(tool) : json_null());
            json_append(row, ts ? json_copy(ts) : json_null());
            json_append(parts, row);
        }
    }
    json_set(out, "parts", parts);
    json_set(out, "in_tool", json_bool(in_tool ? true : false));
    return out;
}

struct json_t *w16_delegateto_2d04d4_sync_note(const char *reason) {
    if (!reason) return json_null();
    if (!strcmp(reason, "no_async"))
        return json_string("background=true is not available in this session — it cannot "
            "receive a detached subagent result after the turn ends (a finite chat using -Q, "
            "--oneshot, or non-TTY stdio, `hermes -z`, a /bg background task, a cron job, a "
            "Kanban worker, or a stateless HTTP endpoint). The subagent(s) ran SYNCHRONOUSLY "
            "and the result is included above.");
    if (!strcmp(reason, "at_capacity"))
        return json_string("The background delegation pool was at capacity "
            "(delegation.max_concurrent_children), so the subagent(s) ran SYNCHRONOUSLY and "
            "the result is included above. Raise delegation.max_concurrent_children in "
            "config.yaml to allow more concurrent background delegations.");
    return json_null();
}

