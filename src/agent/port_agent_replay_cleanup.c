/* Slermes C port — agent/replay_cleanup.py
 * Pure replay-history sanitization shared across all resume surfaces.
 * Operates on a json_t* array of message objects (each a JSON object with a
 * "role" string; assistant messages may carry "tool_calls", tool messages a
 * "content" string). Mirrors the Python list-of-dicts shape exactly.
 *
 * Faithful to the canonical live Python: interrupted/orphan assistant→tool
 * blocks whose calls MAY have side effects are RECOVERED (the interrupted tool
 * result is enriched with an effect_disposition + a recovery sentinel rather
 * than erased), so the model sees an honest "this may have run" marker instead
 * of re-issuing the call. Read-only interrupted blocks are stripped. Also adds
 * strip_stale_dangerous_confirmations (dangerous-confirmation text expiry).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"

/* ── Tool side-effect classification (agent/tool_result_classification.py) ── */
/* Tools whose interrupted/dangling execution is safe to discard because they
 * cannot mutate external or Hermes session state. Unknown/plugin/MCP tools
 * stay effect-capable by default (mirrors Python NO_EFFECT_TOOL_NAMES). */
static bool rc_is_no_effect_tool(const char *name)
{
    if (!name || !name[0]) return false; /* unknown => may have effect */
    /* case-insensitive membership test */
    char low[128];
    size_t n = 0;
    for (const char *p = name; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    static const char *no_effect[] = {
        "read_file", "search_files", "session_search", "skill_view", "skills_list",
        "web_extract", "web_search", "vision_analyze", "browser_snapshot",
        "browser_get_images", "browser_console", "read_terminal", NULL
    };
    for (int i = 0; no_effect[i]; i++)
        if (strcmp(low, no_effect[i]) == 0) return true;
    return false;
}

static bool rc_tool_may_have_side_effect(const char *name)
{
    return !rc_is_no_effect_tool(name);
}

/* ── Interrupted-tool detection (is_interrupted_tool_result) ── */
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

/* Extract a tool_call's function name (nested under "function.name"). */
static char *msg_tool_call_name(const json_t *msg, size_t idx, char *out, size_t outsz)
{
    out[0] = '\0';
    if (!msg || msg->type != JSON_OBJECT) return out;
    json_t *tcs = json_obj_get((json_t *)msg, "tool_calls");
    if (!tcs || tcs->type != JSON_ARRAY) return out;
    json_t *call = json_get(tcs, idx);
    if (!call || call->type != JSON_OBJECT) return out;
    json_t *fn = json_obj_get(call, "function");
    if (fn && fn->type == JSON_OBJECT) {
        json_t *nm = json_obj_get(fn, "name");
        if (nm && nm->type == JSON_STRING && nm->str_val)
            snprintf(out, outsz, "%s", nm->str_val);
    }
    return out;
}

/* Build a recovery tool-result message matching Python make_tool_result_message:
 * {role, name, tool_name, content, tool_call_id, effect_disposition}. */
static json_t *make_recovery_tool_result(const char *name, const char *content,
                                          const char *tool_call_id,
                                          const char *effect_disposition)
{
    json_t *m = json_object();
    if (!m) return NULL;
    json_set(m, "role", json_string("tool"));
    json_set(m, "name", json_string(name && name[0] ? name : "unknown"));
    json_set(m, "tool_name", json_string(name && name[0] ? name : "unknown"));
    json_set(m, "content", json_string(content ? content : ""));
    json_set(m, "tool_call_id", json_string(tool_call_id ? tool_call_id : ""));
    json_set(m, "effect_disposition", json_string(effect_disposition));
    return m;
}

static const char *RECOVERY_UNKNOWN =
    "[Orphan recovery: this tool may have executed before Hermes stopped; "
    "its effect is UNKNOWN. Inspect current state before retrying.]";
static const char *RECOVERY_NONE =
    "[Orphan recovery: this read-only tool did not complete and had no effect.]";
static const char *RECOVERY_INTERRUPTED_UNKNOWN =
    "[Orphan recovery: interrupted side-effecting tool may have "
    "executed; its effect is UNKNOWN. Inspect state before retrying.]";
static const char *RECOVERY_INTERRUPTED_NONE =
    "[Orphan recovery: interrupted read-only tool did not complete.]";

/* PoP: strip_interrupted_tool_tails @ agent/replay_cleanup.py:strip_interrupted_tool_tails */
/* Returns a NEW json_t* array. Interrupted assistant→tool blocks whose calls
 * may have side effects are RECOVERED (interrupted tool results enriched with
 * effect_disposition + recovery sentinel); read-only interrupted blocks are
 * stripped. Caller frees. NULL only on allocation failure. */
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
            if (has_tool && any_interrupted) {
                /* Decide: recover (keep) if ANY call may have side effects. */
                bool may_side_effect = false;
                json_t *tcs = json_obj_get(msg, "tool_calls");
                size_t nt = tcs && tcs->type == JSON_ARRAY ? tcs->c.count : 0;
                for (size_t k = 0; k < nt; k++) {
                    char nm[128];
                    msg_tool_call_name(msg, k, nm, sizeof(nm));
                    if (rc_tool_may_have_side_effect(nm)) { may_side_effect = true; break; }
                }
                if (may_side_effect) {
                    /* Build call_id -> name map for interrupted tool results. */
                    /* Append the assistant message unchanged. */
                    json_array_append(out, json_copy(msg));
                    for (size_t k = i + 1; k < j; k++) {
                        json_t *t = json_get((json_t *)history, k);
                        if (!agent_replay_cleanup_is_interrupted_tool_result(msg_content_str(t))) {
                            json_array_append(out, json_copy(t));
                            continue;
                        }
                        /* Enrich interrupted tool result. */
                        char call_id[128] = "";
                        json_t *cid = t ? json_obj_get(t, "tool_call_id") : NULL;
                        if (cid && cid->type == JSON_STRING && cid->str_val)
                            snprintf(call_id, sizeof(call_id), "%s", cid->str_val);
                        /* Find the call name for this tool_call_id. */
                        char nm[128] = "";
                        for (size_t c = 0; c < nt; c++) {
                            char cn[128];
                            msg_tool_call_name(msg, c, cn, sizeof(cn));
                            json_t *cc = json_get(tcs, c);
                            char ccid[128] = "";
                            if (cc) {
                                json_t *cj = json_obj_get(cc, "id");
                                if (cj && cj->type == JSON_STRING && cj->str_val)
                                    snprintf(ccid, sizeof(ccid), "%s", cj->str_val);
                                else {
                                    json_t *cjid = json_obj_get(cc, "call_id");
                                    if (cjid && cjid->type == JSON_STRING && cjid->str_val)
                                        snprintf(ccid, sizeof(ccid), "%s", cjid->str_val);
                                }
                            }
                            if (strcmp(ccid, call_id) == 0) { snprintf(nm, sizeof(nm), "%s", cn); break; }
                        }
                        const char *disp = rc_tool_may_have_side_effect(nm) ? "unknown" : "none";
                        const char *rec = rc_tool_may_have_side_effect(nm)
                            ? RECOVERY_INTERRUPTED_UNKNOWN : RECOVERY_INTERRUPTED_NONE;
                        json_t *enriched = json_copy(t);
                        if (!enriched) { json_free(out); return NULL; }
                        json_set(enriched, "effect_disposition", json_string(disp));
                        json_set(enriched, "content", json_string(rec));
                        json_array_append(out, enriched);
                    }
                    i = j;
                    continue;
                }
                /* read-only: strip the whole block */
                i = j;
                continue;
            }
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
/* Returns a NEW json_t* array. A trailing unanswered assistant(tool_calls)
 * whose calls may have side effects is RECOVERED as UNKNOWN tool-result
 * messages; a read-only dangling tail is stripped; otherwise unchanged. */
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
    if (!(strcmp(role, "assistant") == 0 && last && json_obj_get(last, "tool_calls")))
        return json_copy(history);

    json_t *tcs = json_obj_get(last, "tool_calls");
    size_t nt = tcs && tcs->type == JSON_ARRAY ? tcs->c.count : 0;

    bool may_side_effect = false;
    for (size_t k = 0; k < nt; k++) {
        char nm[128];
        msg_tool_call_name(last, k, nm, sizeof(nm));
        if (rc_tool_may_have_side_effect(nm)) { may_side_effect = true; break; }
    }
    if (!may_side_effect) {
        /* strip dangling read-only tail */
        json_t *out = json_new_array();
        if (!out) return NULL;
        for (size_t k = 0; k + 1 < n; k++)
            json_array_append(out, json_copy(json_get((json_t *)history, k)));
        return out;
    }

    /* Recover: keep ALL messages (including the dangling assistant call),
     * then append recovery tool-result messages after the tail. */
    json_t *out = json_new_array();
    if (!out) return NULL;
    for (size_t k = 0; k < n; k++)
        json_array_append(out, json_copy(json_get((json_t *)history, k)));
    for (size_t k = 0; k < nt; k++) {
        char nm[128];
        char call_id[128] = "";
        msg_tool_call_name(last, k, nm, sizeof(nm));
        json_t *call = json_get(tcs, k);
        if (call) {
            json_t *cj = json_obj_get(call, "id");
            if (cj && cj->type == JSON_STRING && cj->str_val)
                snprintf(call_id, sizeof(call_id), "%s", cj->str_val);
            else {
                json_t *cjid = json_obj_get(call, "call_id");
                if (cjid && cjid->type == JSON_STRING && cjid->str_val)
                    snprintf(call_id, sizeof(call_id), "%s", cjid->str_val);
            }
        }
        const char *disp = rc_tool_may_have_side_effect(nm) ? "unknown" : "none";
        const char *rec = rc_tool_may_have_side_effect(nm) ? RECOVERY_UNKNOWN : RECOVERY_NONE;
        json_t *tr = make_recovery_tool_result(nm, rec, call_id, disp);
        if (!tr) { json_free(out); return NULL; }
        json_array_append(out, tr);
    }
    return out;
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

/* ── Dangerous-confirmation expiry (#59607) ── */

static bool is_dangerous_confirmation_text(const char *content)
{
    if (!content) return false;
    char low[8192];
    size_t n = 0;
    for (const char *p = content; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    static const char *patterns[] = {
        "confirm forced restart", "confirm forced reboot", "confirm shutdown",
        "confirm reboot", "confirm power off", "yes, delete everything",
        "confirm wipe", "confirm factory reset",
        "確認強制重開機", "確認強制重開", "確認重啟", NULL
    };
    for (int i = 0; patterns[i]; i++)
        if (strstr(low, patterns[i])) return true;
    return false;
}

/* PoP: strip_stale_dangerous_confirmations @ agent/replay_cleanup.py:strip_stale_dangerous_confirmations */
/* Returns a NEW json_t* array; user messages whose dangerous-confirmation text
 * is older than expiry_seconds (and have a numeric "timestamp") are redacted in
 * place (role preserved) with the expired-confirmation sentinel. Caller frees. */
json_t *agent_replay_cleanup_strip_stale_dangerous_confirmations(
    const json_t *history, double now, double expiry_seconds)
{
    if (!history || history->type != JSON_ARRAY) return NULL;
    size_t n = history->c.count;
    json_t *out = json_new_array();
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get((json_t *)history, i);
        char role[32] = "";
        json_t *role_j = msg ? json_obj_get(msg, "role") : NULL;
        if (role_j && role_j->type == JSON_STRING && role_j->str_val)
            snprintf(role, sizeof(role), "%s", role_j->str_val);
        bool is_user = strcmp(role, "user") == 0;
        if (is_user && msg && is_dangerous_confirmation_text(msg_content_str(msg))) {
            json_t *ts = json_obj_get(msg, "timestamp");
            if (ts && (ts->type == JSON_NUMBER || ts->type == JSON_STRING)) {
                double tsval = (ts->type == JSON_NUMBER) ? ts->num_val
                              : atof(ts->str_val ? ts->str_val : "0");
                if ((now - tsval) > expiry_seconds) {
                    json_t *redacted = json_copy(msg);
                    if (!redacted) { json_free(out); return NULL; }
                    json_set(redacted, "content",
                        json_string(
                            "[A high-risk confirmation previously given here has EXPIRED and must "
                            "not be acted on. Ask the user to re-confirm explicitly before "
                            "performing any destructive action.]"));
                    json_array_append(out, redacted);
                    continue;
                }
            }
        }
        if (msg) json_array_append(out, json_copy(msg));
    }
    return out;
}
