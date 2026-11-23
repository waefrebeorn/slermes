/*
 * maintenance.c — real C port of upstream Hermes persistence/housekeeping/maintenance.py constant tables.
 * PoP: cn_a9758b01cb_constants @ persistence/housekeeping/maintenance.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_a9758b01cb.h"

json_t *cn_a9758b01cb_constants(void) {
    json_t *out = json_object();
    json_set(out, "ORPHAN_REAP_END_REASON", json_string("startup_orphan_reap"));
    json_set(out, "GHOST_SESSION_SOURCE", json_string("tui"));
    json_set(out, "AUTO_PRUNE_STALE_OPEN_SOURCES", ({ json_t *__a = json_array();
        json_append(__a, json_string("cli"));
        json_append(__a, json_string("cron"));
        json_append(__a, json_string("kanban"));
        json_append(__a, json_string("acp"));
        json_append(__a, json_string("api_server"));
        json_append(__a, json_string("subagent"));
        json_append(__a, json_string("tool"));
        json_append(__a, json_string("recovered"));
      __a; }));
    json_set(out, "PRUNE_FILTER_NAMES", ({ json_t *__a = json_array();
        json_append(__a, json_string("archived"));
        json_append(__a, json_string("branch_like"));
        json_append(__a, json_string("chat_id"));
        json_append(__a, json_string("chat_type"));
        json_append(__a, json_string("cwd_prefix"));
        json_append(__a, json_string("end_reason"));
        json_append(__a, json_string("include_pinned"));
        json_append(__a, json_string("last_active_after"));
        json_append(__a, json_string("last_active_before"));
        json_append(__a, json_string("lineage_tips_only"));
        json_append(__a, json_string("max_cost"));
        json_append(__a, json_string("max_messages"));
        json_append(__a, json_string("max_tokens"));
        json_append(__a, json_string("max_tool_calls"));
        json_append(__a, json_string("min_cost"));
        json_append(__a, json_string("min_messages"));
        json_append(__a, json_string("min_tokens"));
        json_append(__a, json_string("min_tool_calls"));
        json_append(__a, json_string("model_like"));
        json_append(__a, json_string("provider"));
        json_append(__a, json_string("source"));
        json_append(__a, json_string("started_after"));
        json_append(__a, json_string("started_before"));
        json_append(__a, json_string("title_like"));
        json_append(__a, json_string("user_id"));
      __a; }));
    json_set(out, "IMPORT_MAX_SESSIONS", json_number(500));
    json_set(out, "IMPORT_MAX_MESSAGES_PER_SESSION", json_number(10000));
    json_set(out, "IMPORT_MAX_TOTAL_MESSAGES", json_number(50000));
    json_set(out, "SESSION_PROJECTION_FIELDS", ({ json_t *__a = json_array();
        json_append(__a, json_string("id"));
        json_append(__a, json_string("source"));
        json_append(__a, json_string("session_key"));
        json_append(__a, json_string("display_name"));
        json_append(__a, json_string("model"));
        json_append(__a, json_string("parent_session_id"));
        json_append(__a, json_string("started_at"));
        json_append(__a, json_string("ended_at"));
        json_append(__a, json_string("end_reason"));
        json_append(__a, json_string("message_count"));
        json_append(__a, json_string("tool_call_count"));
        json_append(__a, json_string("cwd"));
        json_append(__a, json_string("git_branch"));
        json_append(__a, json_string("git_repo_root"));
        json_append(__a, json_string("title"));
        json_append(__a, json_string("title_source"));
        json_append(__a, json_string("profile_name"));
        json_append(__a, json_string("archived"));
        json_append(__a, json_string("pinned"));
        json_append(__a, json_string("hidden"));
        json_append(__a, json_string("last_read_at"));
        json_append(__a, json_string("handoff_state"));
      __a; }));
    return out;
}
