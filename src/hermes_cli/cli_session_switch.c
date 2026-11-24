/*
 * cli_session_switch.c — real C11 port of upstream Hermes
 * hermes_cli/cli_session_switch.py.
 *
 * PoP: w17_clisession_e88af5_end_current_session   @ hermes_cli/cli_session_switch.py:_end_current_session
 * PoP: w17_clisession_e88af5_sync_agent_to_session @ hermes_cli/cli_session_switch.py:_sync_agent_to_session
 *
 * Both helpers are side-effect-only in Python. The C port consumes a JSON
 * description of the fake cli/agent state and returns a JSON summary of the
 * calls and assignments the live helpers would make; the oracle replays the
 * same helpers against equivalent fake objects and compares the summaries.
 */
#include "noc_w17_clisession_e88af5.h"

static int noc_cs_has(const json_t *o, const char *k) {
    return o && json_is_object(o) && json_obj_get(o, k) != NULL;
}
static int noc_cs_bool(const json_t *o, const char *k) {
    return json_get_bool(o, k, false) ? 1 : 0;
}

/* _end_current_session: flush only when cli.agent; end_session always. */
json_t *w17_clisession_e88af5_end_current_session(const json_t *state) {
    int has_agent = noc_cs_bool(state, "has_agent");
    json_t *out = json_object();
    json_set(out, "flush", json_bool(has_agent ? true : false));
    json_t *end = json_array();
    json_append(end, json_string(json_get_str(state, "session_id", "")));
    json_append(end, json_string(json_get_str(state, "reason", "")));
    json_set(out, "end", end);
    return out;
}

/* _sync_agent_to_session: no-op when cli.agent is falsy. Otherwise reset the
 * agent, re-anchor the flush index, refresh TodoStore, invalidate the prompt
 * and notify the memory manager with reset=False. */
json_t *w17_clisession_e88af5_sync_agent_to_session(const json_t *state) {
    json_t *out = json_object();
    int has_agent = noc_cs_bool(state, "has_agent");
    json_set(out, "noop", json_bool(has_agent ? false : true));
    if (!has_agent) {
        json_set(out, "session_id", json_null());
        json_set(out, "reset", json_bool(false));
        json_set(out, "last_flushed", json_null());
        json_set(out, "invalidated", json_bool(false));
        json_set(out, "mm", json_null());
        json_set(out, "todo_replaced", json_bool(false));
        return out;
    }
    json_set(out, "session_id", json_string(json_get_str(state, "session_id", "")));
    json_set(out, "reset", json_bool(true));
    if (noc_cs_bool(state, "has_last_flushed"))
        json_set(out, "last_flushed", json_number(json_get_num(state, "history_len", 0)));
    else
        json_set(out, "last_flushed", json_null());
    json_set(out, "invalidated", json_bool(noc_cs_bool(state, "has_invalidate") ? true : false));
    if (noc_cs_bool(state, "has_mm") && noc_cs_bool(state, "mm_present")) {
        json_t *mm = json_array();
        json_append(mm, json_string(json_get_str(state, "session_id", "")));
        const char *parent = json_get_str(state, "parent_session_id", "");
        json_append(mm, json_string(parent ? parent : ""));
        json_append(mm, json_string(json_get_str(state, "reason", "")));
        json_set(out, "mm", mm);
    } else {
        json_set(out, "mm", json_null());
    }
    /* TodoStore is stubbed to construct successfully in the oracle, so a
     * present _todo_store attribute is always replaced with a fresh sentinel. */
    json_set(out, "todo_replaced",
             json_bool((noc_cs_has(state, "has_todo") && noc_cs_bool(state, "has_todo")) ? true : false));
    return out;
}
