/*
 * session_clarify_batch.c — faithful C11 port of upstream Hermes
 * gateway/session_clarify_batch.py (gate w17_sessioncla_88fd4f).
 *
 * PoP: w17_sessioncla_88fd4f_ask_clarify_batch @ gateway/session_clarify_batch.py:ask_clarify_batch
 *
 * Collaborators (ask_viewer_result, get_clarify_timeout, the agent interrupt
 * flag) are scripted via `io` / JSON object doubles.
 */
#include "noc_w17_sessioncla_88fd4f.h"
#include <string.h>
#include <stdlib.h>

static bool scb_null(json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *scb_copy(json_t *v) { return v ? json_copy(v) : json_null(); }
static const char *scb_str(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static void scb_record(json_t *state, const char *key, json_t *ev) {
    json_t *a = json_obj_get(state, key);
    if (!a) { a = json_array(); json_set(state, key, a); }
    json_append(a, ev);
}
static bool scb_in_strs(const char *s, const char *const *v, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, v[i])) return true;
    return false;
}
static json_t *scb_result(const char *outcome, json_t *answers) {
    json_t *o = json_object();
    json_set(o, "answers", answers ? answers : json_object());
    json_set(o, "outcome", json_string(outcome ? outcome : "cancelled"));
    return o;
}

/* PoP: w17_sessioncla_88fd4f_ask_clarify_batch */
json_t *w17_sessioncla_88fd4f_ask_clarify_batch(json_t *turn, json_t *authority,
        json_t *questions, json_t *io, json_t *state) {
    json_t *owner = json_obj_get(turn, "approval_owner");
    const char *sid = NULL;
    if (owner && owner->type == JSON_ARRAY && owner->c.count > 1)
        sid = scb_str(owner->c.items[1]);

    /* live = authority.sessions.get(session_id) */
    json_t *live = json_obj_get(json_obj_get(authority, "sessions"), sid ? sid : "");
    if (scb_null(live)) return scb_result("cancelled", NULL);

    /* interrupted: from agent_holder[0]._interrupt_requested (agent must exist) */
    json_t *holder = json_obj_get(json_obj_get(turn, "ctx"), "agent_holder");
    bool interrupted = false;
    if (holder && holder->type == JSON_ARRAY && holder->c.count > 0) {
        json_t *agent = holder->c.items[0];
        if (!scb_null(agent)) interrupted = json_get_bool(agent, "_interrupt_requested", false);
    }

    /* wire = [{key: entry.get(key) for key in KEYS if entry.get(key) is not None} ...] */
    static const char *const KEYS[4] = {"qid", "question", "choices", "multi_select"};
    json_t *wire = json_array();
    json_t *qids = json_array();
    if (questions && questions->type == JSON_ARRAY) {
        for (size_t i = 0; i < questions->c.count; i++) {
            json_t *q = questions->c.items[i];
            json_t *w = json_object();
            for (size_t k = 0; k < 4; k++) {
                json_t *v = json_obj_get(q, KEYS[k]);
                if (v && v->type != JSON_NULL) json_set(w, KEYS[k], json_copy(v));
            }
            json_append(wire, w);
            json_append(qids, scb_copy(json_obj_get(q, "qid")));
        }
    }

    /* timeout = get_clarify_timeout(); arg = timeout if timeout > 0 else None */
    json_t *tv = json_obj_get(io, "timeout");
    double t = (tv && tv->type == JSON_NUMBER) ? tv->num_val : 0.0;
    json_t *timeout_arg = (t > 0.0) ? scb_copy(tv) : json_null();

    /* ask_viewer_result(live.event_stream, session_id, "clarify", {"questions": wire},
     *                   cancelled=interrupted, timeout=..., qids=[...]) */
    json_t *ev = json_object();
    json_set(ev, "event_stream", scb_copy(json_obj_get(live, "event_stream")));
    json_set(ev, "session_id", json_string(sid ? sid : ""));
    json_set(ev, "kind", json_string("clarify"));
    json_t *params = json_object();
    json_set(params, "questions", scb_copy(wire));
    json_set(ev, "params", params);
    json_set(ev, "cancelled", json_bool(interrupted));
    json_set(ev, "timeout", scb_copy(timeout_arg));
    json_set(ev, "qids", scb_copy(qids));
    scb_record(state, "ask_viewer_calls", ev);
    json_free(wire);
    json_free(qids);

    json_t *result = json_obj_get(io, "result");
    if (scb_null(result)) {
        json_t *a = json_object();
        return scb_result(interrupted ? "cancelled" : "timed_out", a);
    }
    json_t *answers = json_obj_get(result, "answers");
    if (!answers || answers->type != JSON_OBJECT) return scb_result("cancelled", NULL);

    /* known = {entry["qid"] for entry in questions}; keep answer order */
    json_t *clean = json_object();
    for (size_t i = 0; i < answers->c.count; i++) {
        const char *k = answers->c.keys[i];
        json_t *av = answers->c.items[i];
        bool known = false;
        if (questions && questions->type == JSON_ARRAY) {
            for (size_t j = 0; j < questions->c.count; j++) {
                const char *qid = scb_str(json_obj_get(questions->c.items[j], "qid"));
                if (qid && !strcmp(k, qid)) { known = true; break; }
            }
        }
        if (!known) continue;
        json_set(clean, k, (av && av->type == JSON_STRING) ? json_copy(av) : json_null());
    }
    static const char *const OUTCOMES[3] = {"submitted", "timed_out", "cancelled"};
    const char *oc = scb_str(json_obj_get(result, "outcome"));
    if (!scb_in_strs(oc, OUTCOMES, 3)) oc = "cancelled";
    return scb_result(oc, clean);
}
