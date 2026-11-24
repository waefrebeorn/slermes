/*
 * session_deferred_pick.c — faithful C11 port of upstream Hermes
 * gateway/session_deferred_pick.py (gate w17_sessiondef_feb26c).
 *
 * PoP: w17_sessiondef_feb26c_defer_pick @ gateway/session_deferred_pick.py:defer_pick
 * PoP: w17_sessiondef_feb26c_reask_on_attach @ gateway/session_deferred_pick.py:reask_on_attach
 * PoP: w17_sessiondef_feb26c_commit @ gateway/session_deferred_pick.py:_commit
 * PoP: w17_sessiondef_feb26c_commit_level @ gateway/session_deferred_pick.py:_commit_level
 * PoP: w17_sessiondef_feb26c_apply_deferred_pick @ gateway/session_deferred_pick.py:apply_deferred_pick
 *
 * Collaborators (authority_persistence, write_mutation, runtime seams) are
 * scripted via `io`; every seam call and mutated live field is recorded in
 * `state` so the differential oracle observes all arguments.
 */
#include "noc_w17_sessiondef_feb26c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static bool sd_null(json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *sd_copy(json_t *v) { return v ? json_copy(v) : json_null(); }
static const char *sd_str(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static void sd_record(json_t *state, const char *key, json_t *ev) {
    json_t *a = json_obj_get(state, key);
    if (!a) { a = json_array(); json_set(state, key, a); }
    json_append(a, ev);
}
static json_t *sd_raise(const char *cls) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    return o;
}
static bool sd_is_raise(json_t *v, const char *cls) {
    if (!v || v->type != JSON_OBJECT) return false;
    json_t *r = json_obj_get(v, "$raise");
    return r && r->type == JSON_STRING && strcmp(r->str_val, cls) == 0;
}
static json_t *sd_live_state(json_t *live) {
    json_t *o = json_object();
    json_set(o, "reask_pick", json_bool(json_is_true(json_obj_get(live, "reask_pick"))));
    return o;
}

/* PoP: w17_sessiondef_feb26c_defer_pick */
json_t *w17_sessiondef_feb26c_defer_pick(json_t *authority, json_t *actor, const char *sid,
        json_t *params, json_t *io, json_t *state) {
    const char *op = json_get_str(params, "operation", "model");
    json_t *pm = json_obj_get(io, "parse_mutation");
    if (json_is_object(pm) && json_obj_get(pm, "$raise"))
        return sd_raise(sd_str(json_obj_get(pm, "$raise")) ? sd_str(json_obj_get(pm, "$raise")) : "ValueError");

    json_t *shown = json_null();
    if (!strcmp(op, "model") && json_get_bool(io, "is_model_reset", false))
        shown = sd_copy(json_obj_get(io, "configured_default"));

    json_t *rk = json_obj_get(io, "receipt_key");
    json_t *pick = json_object();
    json_set(pick, "operation", json_string(op));
    json_set(pick, "principal_id", sd_copy(json_obj_get(actor, "subject")));
    json_set(pick, "request_id", sd_copy(json_obj_get(params, "request_id")));
    json_set(pick, "payload", sd_copy(json_obj_get(params, "payload")));
    json_set(pick, "shown", sd_copy(shown));

    json_t *ev = json_object();
    json_set(ev, "epoch", sd_copy(json_obj_get(authority, "epoch")));
    json_set(ev, "key", sd_copy(json_obj_get(rk, "key")));
    json_set(ev, "digest", sd_copy(json_obj_get(rk, "digest")));
    json_set(ev, "session_id", json_string(sid));
    json_set(ev, "pick", pick);
    sd_record(state, "defer_model_pick_calls", ev);

    return sd_copy(json_obj_get(io, "defer_result"));
}

/* PoP: w17_sessiondef_feb26c_reask_on_attach */
json_t *w17_sessiondef_feb26c_reask_on_attach(json_t *authority, const char *sid,
        json_t *live, json_t *io, json_t *state) {
    (void)authority;
    json_t *stored = json_obj_get(io, "stored");
    if (!sd_null(stored)) {
        json_t *pick = json_obj_get(stored, "pick");
        if (json_obj_get(pick, "awaiting_confirm")) {
            json_set(live, "reask_pick", json_bool(true));
            json_t *ev = json_object();
            json_set(ev, "session_id", json_string(sid));
            sd_record(state, "schedule_calls", ev);
        }
    }
    json_set(state, "live", sd_live_state(live));
    return json_null();
}

/* PoP: w17_sessiondef_feb26c_commit */
json_t *w17_sessiondef_feb26c_commit(json_t *authority, const char *sid, json_t *entry,
        const char *operation, json_t *io, json_t *state) {
    (void)authority;
    json_t *hr = json_object();
    json_set(hr, "principal_id", sd_copy(json_obj_get(entry, "principal_id")));
    json_set(hr, "session_id", json_string(sid));
    json_set(hr, "application_id", sd_copy(json_obj_get(entry, "application_id")));
    sd_record(state, "has_receipt_calls", hr);
    if (json_get_bool(io, "has_receipt", false)) {
        json_t *o = json_object(); json_set(o, "status", json_string("replayed")); return o;
    }
    json_t *row = json_obj_get(io, "row");
    json_t *mparams = json_object();
    json_set(mparams, "session_id", json_string(sid));
    json_set(mparams, "request_id", sd_copy(json_obj_get(entry, "application_id")));
    json_set(mparams, "expected_revision", sd_copy(json_obj_get(row, "runtime_revision")));
    json_set(mparams, "expected_generation", sd_copy(json_obj_get(row, "runtime_generation")));
    json_set(mparams, "operation", json_string(operation));
    json_set(mparams, "payload", sd_copy(json_obj_get(entry, "payload")));
    json_t *wm = json_object();
    json_set(wm, "params", mparams);
    json_set(wm, "principal_id", sd_copy(json_obj_get(entry, "principal_id")));
    sd_record(state, "write_mutation_calls", wm);

    json_t *wr = json_obj_get(io, "write_result");
    if (sd_is_raise(wr, "GuardRefusal")) return json_null();
    if (sd_is_raise(wr, "AuthorityRefusal")) {
        const char *reason = json_get_str(wr, "reason", "refused");
        if (!strcmp(reason, "session_busy")) return json_null();
        json_t *o = json_object();
        json_set(o, "session_id", json_string(sid));
        json_set(o, "operation", json_string(operation));
        json_set(o, "status", json_string("refused"));
        json_set(o, "reason", json_string(reason));
        return o;
    }
    if (sd_is_raise(wr, "Exception") || sd_is_raise(wr, "RuntimeError")) {
        json_t *o = json_object();
        json_set(o, "session_id", json_string(sid));
        json_set(o, "operation", json_string(operation));
        json_set(o, "status", json_string("refused"));
        char buf[160];
        snprintf(buf, sizeof(buf), "%s_resolution_failed", operation);
        json_set(o, "reason", json_string(buf));
        return o;
    }
    return sd_copy(wr);
}

/* PoP: w17_sessiondef_feb26c_commit_level */
json_t *w17_sessiondef_feb26c_commit_level(json_t *authority, const char *sid, json_t *raw,
        json_t *pick, json_t *io, json_t *state) {
    json_t *pc = json_copy(pick);
    if (!pc) pc = json_object();
    json_t *reasoning = json_obj_get(pc, "reasoning");
    json_t *reasoning_cp = sd_copy(reasoning);
    json_obj_del(pc, "reasoning");
    json_t *result = w17_sessiondef_feb26c_commit(authority, sid, reasoning_cp, "reasoning", io, state);
    if (sd_null(result)) { json_free(pc); json_free(reasoning_cp); return json_null(); }
    if (json_is_true(json_obj_get(result, "status"))
        || (json_obj_get(result, "status") && sd_str(json_obj_get(result, "status")) &&
            !strcmp(sd_str(json_obj_get(result, "status")), "refused"))) {
        /* logger.warning — observationally silent */
    }
    json_t *ret;
    if (!json_obj_get(pc, "application_id")) {
        json_t *ev = json_object();
        json_set(ev, "op", json_string("forget_deferred_pick"));
        json_set(ev, "stored", sd_copy(raw));
        sd_record(state, "runtime_calls", ev);
        ret = json_null();
    } else {
        json_t *ev = json_object();
        json_set(ev, "op", json_string("replace_deferred_pick"));
        json_set(ev, "stored", sd_copy(raw));
        json_set(ev, "pick", sd_copy(pc));
        sd_record(state, "runtime_calls", ev);
        ret = sd_copy(json_obj_get(io, "replace_result"));
        if (sd_null(ret)) ret = sd_copy(raw);
    }
    json_free(pc);
    json_free(reasoning_cp);
    json_free(result);
    return ret;
}

/* PoP: w17_sessiondef_feb26c_apply_deferred_pick */
json_t *w17_sessiondef_feb26c_apply_deferred_pick(json_t *authority, const char *sid,
        json_t *live, json_t *io, json_t *state) {
    json_t *stored = json_obj_get(io, "stored");
    if (sd_null(stored)) return json_null();
    json_t *raw = sd_copy(json_obj_get(stored, "stored"));
    json_t *pick = json_copy(json_obj_get(stored, "pick"));
    if (!pick) pick = json_object();

    if (json_obj_get(pick, "reasoning")) {
        json_t *nr = w17_sessiondef_feb26c_commit_level(authority, sid, raw, pick, io, state);
        json_free(raw);
        raw = nr;
        if (sd_null(raw)) { json_free(pick); return json_null(); }
    }
    if (json_obj_get(pick, "awaiting_confirm")) {
        if (!json_is_true(json_obj_get(live, "reask_pick"))) { json_free(pick); json_free(raw); return json_null(); }
        json_obj_del(pick, "awaiting_confirm");
    }
    json_set(live, "reask_pick", json_bool(false));
    json_set(state, "live", sd_live_state(live));

    json_t *result = w17_sessiondef_feb26c_commit(authority, sid, pick, "model", io, state);
    if (sd_null(result)) { json_free(pick); json_free(raw); return json_null(); }
    const char *status = sd_str(json_obj_get(result, "status"));
    if (status && !strcmp(status, "confirmation_required")) {
        json_t *ev = json_object();
        json_set(ev, "op", json_string("mark_deferred_pick_asked"));
        json_set(ev, "stored", sd_copy(raw));
        json_set(ev, "confirm", sd_copy(json_obj_get(result, "confirm")));
        sd_record(state, "runtime_calls", ev);
        json_t *current = json_obj_get(io, "current");
        const char *provider = current ? sd_str(json_obj_get(json_obj_get(current, "model_config"), "provider")) : NULL;
        json_t *payload = json_copy(result);
        json_set(payload, "payload", sd_copy(json_obj_get(pick, "payload")));
        json_set(payload, "committed_model", current ? sd_copy(json_obj_get(current, "model")) : json_null());
        if (provider) json_set(payload, "committed_provider", json_string(provider));
        json_t *pe = json_object();
        json_set(pe, "session_id", json_string(sid));
        json_set(pe, "payload", payload);
        json_set(pe, "event_type", json_string("session.updated"));
        sd_record(state, "publish_calls", pe);
        json_free(result); json_free(pick); json_free(raw);
        return json_null();
    }
    json_t *fe = json_object();
    json_set(fe, "op", json_string("forget_deferred_pick"));
    json_set(fe, "stored", sd_copy(raw));
    sd_record(state, "runtime_calls", fe);
    if (status && !strcmp(status, "refused")) {
        json_t *current = json_obj_get(io, "current");
        const char *provider = current ? sd_str(json_obj_get(json_obj_get(current, "model_config"), "provider")) : NULL;
        json_t *payload = json_copy(result);
        json_t *pl = json_obj_get(pick, "payload");
        json_set(payload, "target_model", sd_copy(json_obj_get(pl, "model")));
        json_set(payload, "model", current ? sd_copy(json_obj_get(current, "model")) : json_null());
        if (provider) json_set(payload, "provider", json_string(provider));
        json_t *pe = json_object();
        json_set(pe, "session_id", json_string(sid));
        json_set(pe, "payload", payload);
        json_set(pe, "event_type", json_string("session.updated"));
        sd_record(state, "publish_calls", pe);
    }
    json_free(result); json_free(pick); json_free(raw);
    return json_null();
}
