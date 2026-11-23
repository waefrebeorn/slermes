/*
 * turn_facade.c — C home for upstream Hermes agent/turn_facade.py.
 *
 * PoP: w6_turnfacade_bf7ace_chat @ agent/turn_facade.py:TurnFacadeMixin.chat
 * PoP: w6_turnfacade_bf7ace_outcome @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * PoP: w6_turnfacade_bf7ace_exception_outcome @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * PoP: w6_turnfacade_bf7ace_task_context @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * PoP: w6_turnfacade_bf7ace_relay_turn_id @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 * PoP: w6_turnfacade_bf7ace_relay_parent @ agent/turn_facade.py:TurnFacadeMixin.run_conversation
 *
 * The mixin's turn orchestration is I/O-bound; these are its deterministic
 * pure seams, driven by injected values so they can be replayed against the
 * live Python without threads, network or a session database.
 */
#include "noc_w6_turnfacade_bf7ace.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *s_or_empty(const char *s) { return s ? s : ""; }

struct json_t *w6_turnfacade_bf7ace_chat(const struct json_t *result)
{
    if (result && result->type == JSON_OBJECT) {
        const struct json_t *v = json_obj_get(result, "final_response");
        if (v) return json_copy(v);
    }
    return json_null();  /* Python: KeyError -> oracle maps to None */
}

const char *w6_turnfacade_bf7ace_outcome(const struct json_t *terminal)
{
    if (terminal && terminal->type == JSON_OBJECT) {
        const struct json_t *v = json_obj_get(terminal, "interrupted");
        if (v && v->type == JSON_BOOL && v->bool_val) return "cancelled";
        v = json_obj_get(terminal, "failed");
        if (v && v->type == JSON_BOOL && v->bool_val) return "failed";
    }
    return "success";
}

const char *w6_turnfacade_bf7ace_exception_outcome(const char *exc_type)
{
    const char *t = s_or_empty(exc_type);
    if (!strcmp(t, "KeyboardInterrupt") || !strcmp(t, "InterruptedError") ||
        !strcmp(t, "CancelledError")) return "cancelled";
    if (!strcmp(t, "TimeoutError")) return "timed_out";
    return "failed";
}

struct json_t *w6_turnfacade_bf7ace_task_context(const char *session_id,
                                                 const char *task_id,
                                                 const char *platform)
{
    struct json_t *out = json_object();
    json_set(out, "session_id", json_string(s_or_empty(session_id)));
    json_set(out, "task_id", json_string(s_or_empty(task_id)));
    json_set(out, "platform", json_string(s_or_empty(platform)));
    return out;
}

char *w6_turnfacade_bf7ace_relay_turn_id(const char *session_id, const char *task_id,
                                         const char *uuid_hex)
{
    const char *sid = s_or_empty(session_id);
    if (!*sid) sid = "session";
    const char *hex = s_or_empty(uuid_hex);
    size_t hn = strlen(hex);
    if (hn > 8) hn = 8;  /* uuid4().hex[:8] */
    size_t n = strlen(sid) + 1 + strlen(s_or_empty(task_id)) + 1 + hn;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    snprintf(out, n + 1, "%s:%s:%.*s", sid, s_or_empty(task_id), (int)hn, hex);
    return out;
}

char *w6_turnfacade_bf7ace_relay_parent(const char *parent_session_id, const char *platform)
{
    const char *v = (!strcmp(s_or_empty(platform), "subagent")) ? s_or_empty(parent_session_id) : "";
    size_t n = strlen(v);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, v, n);
    out[n] = '\0';
    return out;
}
