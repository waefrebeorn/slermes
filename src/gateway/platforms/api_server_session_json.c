/*
 * api_server_session_json.c - session/message JSON helpers.
 *
 * Split from api_server_adapter_sessions.c (soul.md: self-contained
 * modules). Session / message serialisation and the nonnegative-int parser.
 * No file-scope state.
 */
#include "api_server_session_json.h"
#include "../lib/libdb/db.h"  /* Use libdb directly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* Port of Python hermes_cli/goals.py:to_json(). */
/* ── Helper: Session JSON Response ────────────────────────────────── */ json_t *session_to_json(db_t *db, const char *session_id)
{
    session_meta_t meta;
    char *session_data = db_load(db, session_id, NULL);
    if (!session_data) return NULL;

    json_t *root = json_object();
    json_set(root, "id", json_string(session_id));

    if (db_load_meta(db, session_id, &meta)) {
        json_set(root, "source", json_string(meta.source));
        json_set(root, "model", json_string(meta.model));
        json_set(root, "title", json_string(meta.title));
        json_set(root, "started_at", json_number((double)meta.created_at));
        json_set(root, "ended_at", json_number((double)meta.ended_at));
        if (meta.end_reason[0]) json_set(root, "end_reason", json_string(meta.end_reason));
        json_set(root, "message_count", json_number(meta.message_count));
        json_set(root, "tool_call_count", json_number(meta.tool_call_count));
        json_set(root, "input_tokens", json_number(meta.input_tokens));
        json_set(root, "output_tokens", json_number(meta.output_tokens));
        json_set(root, "cache_read_tokens", json_number(meta.cache_read_tokens));
        json_set(root, "cache_write_tokens", json_number(meta.cache_write_tokens));
        json_set(root, "reasoning_tokens", json_number(meta.reasoning_tokens));
        json_set(root, "estimated_cost_usd", json_number(meta.estimated_cost));
        json_set(root, "actual_cost_usd", json_number(0.0));  /* Not tracked separately */
        json_set(root, "api_call_count", json_number(0));      /* Not tracked */
        if (meta.parent_id[0]) json_set(root, "parent_session_id", json_string(meta.parent_id));
        json_set(root, "last_active", json_number((double)meta.updated_at));
        /* preview and lineage_root_id not in libdb meta */
        json_set(root, "has_system_prompt", json_bool(false));  /* Would check session data */
        json_set(root, "has_model_config", json_bool(false));
    } else {
        /* Fallback if no meta file */
        json_set(root, "source", json_string("unknown"));
        json_set(root, "model", json_string(""));
        json_set(root, "title", json_string(""));
        json_set(root, "started_at", json_number(0));
        json_set(root, "ended_at", json_number(0));
        json_set(root, "message_count", json_number(0));
        json_set(root, "tool_call_count", json_number(0));
        json_set(root, "input_tokens", json_number(0));
        json_set(root, "output_tokens", json_number(0));
        json_set(root, "cache_read_tokens", json_number(0));
        json_set(root, "cache_write_tokens", json_number(0));
        json_set(root, "reasoning_tokens", json_number(0));
        json_set(root, "estimated_cost_usd", json_number(0.0));
        json_set(root, "actual_cost_usd", json_number(0.0));
        json_set(root, "api_call_count", json_number(0));
        json_set(root, "last_active", json_number(0));
        json_set(root, "has_system_prompt", json_bool(false));
        json_set(root, "has_model_config", json_bool(false));
    }

    free(session_data);
    return root;
}
 json_t *message_to_json(const char *msg_json)
{
    /* Parse message from JSON string */
    char *err = NULL;
    json_t *msg = json_parse(strdup(msg_json), &err);
    if (!msg) return NULL;

    json_t *root = json_object();
    const char *id = json_get_str(msg, "id", "");
    const char *session_id = json_get_str(msg, "session_id", "");
    const char *role = json_get_str(msg, "role", "unknown");
    const char *content = json_get_str(msg, "content", "");
    const char *tool_call_id = json_get_str(msg, "tool_call_id", "");
    const char *tool_calls = json_get_str(msg, "tool_calls", "");
    const char *tool_name = json_get_str(msg, "tool_name", "");
    double timestamp = json_get_num(msg, "timestamp", 0);
    int token_count = (int)json_get_num(msg, "token_count", 0);
    const char *finish_reason = json_get_str(msg, "finish_reason", "");
    const char *reasoning = json_get_str(msg, "reasoning", "");
    const char *reasoning_content = json_get_str(msg, "reasoning_content", "");

    json_set(root, "id", json_string(id));
    json_set(root, "session_id", json_string(session_id));
    json_set(root, "role", json_string(role));
    json_set(root, "content", json_string(content));
    if (tool_call_id[0]) json_set(root, "tool_call_id", json_string(tool_call_id));
    if (tool_calls[0]) json_set(root, "tool_calls", json_string(tool_calls));
    if (tool_name[0]) json_set(root, "tool_name", json_string(tool_name));
    json_set(root, "timestamp", json_number(timestamp));
    if (token_count > 0) json_set(root, "token_count", json_number(token_count));
    if (finish_reason[0]) json_set(root, "finish_reason", json_string(finish_reason));
    if (reasoning[0]) json_set(root, "reasoning", json_string(reasoning));
    if (reasoning_content[0]) json_set(root, "reasoning_content", json_string(reasoning_content));

    json_free(msg);
    return root;
}


/* Port of Python gateway/platforms/api_server.py:_parse_nonnegative_int(). */ int parse_nonnegative_int(const char *value, int default_val, int maximum)
{
    if (!value) return default_val;
    char *endptr;
    long n = strtol(value, &endptr, 10);
    if (endptr == value || n < 0) return default_val;
    return (n > maximum) ? maximum : (int)n;
}
