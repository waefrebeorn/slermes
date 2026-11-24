/*
 * session_acp_lifecycle.c — faithful C11 port of upstream Hermes
 * gateway/session_acp_lifecycle.py (gate w17_sessionacp_b27cf9).
 *
 * PoP: w17_sessionacp_b27cf9_end_idle_acp_session @ gateway/session_acp_lifecycle.py:end_idle_acp_session
 * PoP: w17_sessionacp_b27cf9_end_idle_viewer_session @ gateway/session_acp_lifecycle.py:end_idle_viewer_session
 * PoP: w17_sessionacp_b27cf9_reopen_if_reattached @ gateway/session_acp_lifecycle.py:reopen_if_reattached
 * PoP: w17_sessionacp_b27cf9_end_idle_acp_sessions @ gateway/session_acp_lifecycle.py:end_idle_acp_sessions
 *
 * Collaborators (authority_persistence / bindings / logging) are scripted via
 * the `io` object; all observable seam arguments are recorded into `state`.
 */
#include "noc_w17_sessionacp_b27cf9.h"
#include <string.h>
#include <stdlib.h>

#define SACP_ACP_REASON "acp_disconnect"

static bool sacp_truthy(json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count != 0;
        case JSON_OBJECT: return v->c.count != 0;
    }
    return false;
}
static json_t *sacp_copy(json_t *v) { return v ? json_copy(v) : json_null(); }

static void sacp_record(json_t *state, const char *key, json_t *event) {
    json_t *arr = json_obj_get(state, key);
    if (!arr) { arr = json_array(); json_set(state, key, arr); }
    json_append(arr, event);
}

/* receipt(sid): 0 = ok (borrowed receipt), 1 = AuthorityRefusal */
static json_t *sacp_receipt(json_t *io, const char *sid, bool *raised) {
    json_t *spec = json_obj_get(json_obj_get(io, "receipts"), sid);
    *raised = false;
    if (!spec || (json_is_object(spec) && json_obj_get(spec, "$raise"))) {
        *raised = true;
        return NULL;
    }
    return spec;
}

/* bindings.end_idle(epoch,sid,target,reason): returns bool result, *raised on exception. */
static bool sacp_end_idle(json_t *authority, const char *sid, const char *target_id,
                          const char *reason, json_t *io, json_t *state, bool *raised) {
    json_t *args = json_object();
    json_set(args, "epoch", sacp_copy(json_obj_get(authority, "epoch")));
    json_set(args, "session_id", json_string(sid));
    json_set(args, "target_id", json_string(target_id));
    json_set(args, "reason", json_string(reason));
    sacp_record(state, "end_idle_calls", args);

    json_t *spec = json_obj_get(json_obj_get(io, "end_idle"), sid);
    *raised = json_is_object(spec) && json_obj_get(spec, "$raise") != NULL;
    return *raised ? false : sacp_truthy(spec);
}

static bool sacp_has_subscribers(json_t *authority, const char *sid) {
    json_t *live = json_obj_get(json_obj_get(authority, "sessions"), sid);
    if (!live || live->type != JSON_OBJECT) return false;
    json_t *subs = json_obj_get(live, "subscribers");
    return subs && subs->type == JSON_ARRAY && subs->c.count != 0;
}

/* Core of end_idle_acp_session, parameterised by reason (viewer ACP branch). */
static json_t *sacp_end_idle_core(json_t *authority, const char *sid, const char *reason,
                                  json_t *io, json_t *state) {
    if (sacp_has_subscribers(authority, sid)) return json_bool(false);
    bool raised;
    json_t *receipt = sacp_receipt(io, sid, &raised);
    if (raised) return json_bool(false);
    const char *src = json_get_str(json_obj_get(receipt, "policy"), "source", "");
    if (strcmp(src, "acp") != 0) return json_bool(false);
    json_t *target = json_obj_get(json_obj_get(receipt, "entry"), "session_id");
    if (!target || target->type == JSON_NULL) return json_bool(false); /* KeyError */
    const char *tgt = target->type == JSON_STRING ? target->str_val : "";
    bool era;
    bool ok = sacp_end_idle(authority, sid, tgt, reason, io, state, &era);
    return json_bool(!era && ok);
}

/* PoP: w17_sessionacp_b27cf9_end_idle_acp_session */
json_t *w17_sessionacp_b27cf9_end_idle_acp_session(json_t *authority, const char *sid,
        json_t *io, json_t *state) {
    return sacp_end_idle_core(authority, sid, SACP_ACP_REASON, io, state);
}

/* PoP: w17_sessionacp_b27cf9_end_idle_viewer_session */
json_t *w17_sessionacp_b27cf9_end_idle_viewer_session(json_t *authority, const char *sid,
        const char *reason, json_t *io, json_t *state) {
    if (sacp_has_subscribers(authority, sid)) return json_bool(false);
    bool raised;
    json_t *receipt = sacp_receipt(io, sid, &raised);
    if (raised) return json_bool(false);
    const char *src = json_get_str(json_obj_get(receipt, "policy"), "source", "");
    if (strcmp(src, "acp") == 0)
        return sacp_end_idle_core(authority, sid, SACP_ACP_REASON, io, state);
    json_t *target = json_obj_get(json_obj_get(receipt, "entry"), "session_id");
    if (!target || target->type == JSON_NULL) return json_bool(false);
    const char *tgt = target->type == JSON_STRING ? target->str_val : "";
    bool era;
    bool ok = sacp_end_idle(authority, sid, tgt, reason, io, state, &era);
    return json_bool(!era && ok);
}

/* PoP: w17_sessionacp_b27cf9_reopen_if_reattached */
json_t *w17_sessionacp_b27cf9_reopen_if_reattached(json_t *authority, const char *sid,
        bool ended, json_t *io, json_t *state) {
    if (ended && sacp_has_subscribers(authority, sid)) {
        bool raised;
        json_t *receipt = sacp_receipt(io, sid, &raised);
        (void)raised;
        json_t *target = receipt ? json_obj_get(json_obj_get(receipt, "entry"), "session_id") : NULL;
        json_t *call = json_object();
        json_set(call, "target", sacp_copy(target));
        sacp_record(state, "reopen_calls", call);
    }
    return json_bool(ended);
}

/* PoP: w17_sessionacp_b27cf9_end_idle_acp_sessions */
json_t *w17_sessionacp_b27cf9_end_idle_acp_sessions(json_t *authority, json_t *io, json_t *state) {
    double total = 0.0;
    json_t *ids = json_obj_get(io, "local_session_ids");
    if (ids && ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < ids->c.count; i++) {
            json_t *idv = ids->c.items[i];
            const char *sid = (idv && idv->type == JSON_STRING) ? idv->str_val : "";
            json_t *r = sacp_end_idle_core(authority, sid, SACP_ACP_REASON, io, state);
            if (json_is_true(r)) total += 1.0;
            json_free(r);
        }
    }
    return json_number(total);
}
