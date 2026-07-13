/* Slermes C port — agent/replay_cleanup.py
 * Pure replay-history sanitization shared across all resume surfaces.
 * Operates on a json_t* array of message objects (each a JSON object with a
 * "role" string; assistant messages may carry "tool_calls", tool messages a
 * "content" string). Mirrors the Python list-of-dicts shape exactly. */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: is_interrupted_tool_result @ agent/replay_cleanup.py:is_interrupted_tool_result */
bool agent_replay_cleanup_is_interrupted_tool_result(const char *content)
{
    if (!content) return false;
    char low[8192];
    size_t n = 0;
    for (const char *p = content; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    if (strstr(low, "[command interrupted]")) return true;
    if (strstr(low, "exit_code") && (strstr(low, "130") || strstr(low, "-1"))) {
        if (strstr(low, "interrupt")) return true;
    }
    return false;
}

/* Read a message's content as a C string (tool results are plain strings in
 * the C history; fall back to empty). */
static const char *msg_content_str(const json_t *msg)
{
    if (!msg || msg->type != JSON_OBJECT) return "";
    json_t *c = json_obj_get((json_t *)msg, "content");
    if (c && c->type == JSON_STRING) return c->str_val ? c->str_val : "";
    return "";
}

/* PoP: strip_interrupted_tool_tails @ agent/replay_cleanup.py:strip_interrupted_tool_tails */
/* Returns a NEW json_t* array with interrupted assistant->tool blocks removed.
 * Caller frees. Returns NULL only on allocation failure. */
json_t *agent_replay_cleanup_strip_interrupted_tool_tails(const json_t *history)
{
    if (!history || history->type != JSON_ARRAY) return NULL;
    size_t n = history->c.count;
    json_t *out = json_new_array();
    if (!out) return NULL;
    for (size_t i = 0; i < n; ) {
        json_t *msg = json_get((json_t *)history, i);
        char role[32] = "";
        json_t *role_j = msg ? json_obj_get(msg, "role") : NULL;
        if (role_j && role_j->type == JSON_STRING && role_j->str_val)
            snprintf(role, sizeof(role), "%s", role_j->str_val);

        if (strcmp(role, "assistant") == 0 && msg &&
            json_obj_get(msg, "tool_calls")) {
            /* gather following contiguous tool messages */
            size_t j = i + 1;
            bool has_tool = false, any_interrupted = false;
            for (; j < n; j++) {
                json_t *t = json_get((json_t *)history, j);
                char tr[32] = "";
                json_t *trj = t ? json_obj_get(t, "role") : NULL;
                if (trj && trj->type == JSON_STRING && trj->str_val)
                    snprintf(tr, sizeof(tr), "%s", trj->str_val);
                if (strcmp(tr, "tool") != 0) break;
                has_tool = true;
                if (agent_replay_cleanup_is_interrupted_tool_result(msg_content_str(t)))
                    any_interrupted = true;
            }
            if (has_tool && any_interrupted) { i = j; continue; }
        }
        if (strcmp(role, "tool") == 0 && msg &&
            agent_replay_cleanup_is_interrupted_tool_result(msg_content_str(msg))) {
            i++; continue; /* orphan interrupted tool result */
        }
        if (msg) json_array_append(out, json_copy(msg));
        i++;
    }
    return out;
}

/* PoP: strip_dangling_tool_call_tail @ agent/replay_cleanup.py:strip_dangling_tool_call_tail */
/* Returns a NEW json_t* array with a trailing unanswered assistant(tool_calls)
 * removed (returns input clone when nothing to strip). Caller frees. */
json_t *agent_replay_cleanup_strip_dangling_tool_call_tail(const json_t *history)
{
    if (!history || history->type != JSON_ARRAY) return NULL;
    size_t n = history->c.count;
    if (n == 0) return json_copy(history);
    json_t *last = json_get((json_t *)history, n - 1);
    char role[32] = "";
    json_t *role_j = last ? json_obj_get(last, "role") : NULL;
    if (role_j && role_j->type == JSON_STRING && role_j->str_val)
        snprintf(role, sizeof(role), "%s", role_j->str_val);
    if (strcmp(role, "assistant") == 0 && last && json_obj_get(last, "tool_calls")) {
        /* only strip if NO following tool result exists (tail only) */
        json_t *out = json_new_array();
        if (!out) return NULL;
        for (size_t k = 0; k + 1 < n; k++)
            json_array_append(out, json_copy(json_get((json_t *)history, k)));
        return out;
    }
    return json_copy(history);
}

/* PoP: sanitize_replay_history @ agent/replay_cleanup.py:sanitize_replay_history */
json_t *agent_replay_cleanup_sanitize_replay_history(const json_t *history)
{
    if (!history || history->type != JSON_ARRAY) return NULL;
    json_t *a = agent_replay_cleanup_strip_interrupted_tool_tails(history);
    json_t *b = agent_replay_cleanup_strip_dangling_tool_call_tail(a);
    json_free(a);
    return b;
}
