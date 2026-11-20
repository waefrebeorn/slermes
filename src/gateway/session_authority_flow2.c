/*
 * session_authority_flow2.c — REMAINING unclaimed FLOW cores of
 * gateway/session_authority.py (hermes-agent): the routing/register half
 * (reserve/publish/name_lane/agent), the native admission write, the
 * receipt/cancel/resolve-unknown chains, interrupt, head refusal, native
 * recovery, automation admission and the submit pipeline.
 *
 * Decision sequences only: every live I/O (store reads/writes, restore seams,
 * event-stream publishes, runtime workers, owner identity, media leases,
 * logging) enters through ONE universal seam: sau2_call(ctx, name, args). The
 * harness wires it to a recording stub fed the case's scripted results (in.io);
 * the oracle installs recording monkeypatch stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts. Refusals mirror the real
 * AuthorityRefusal (reason/field/detail, _one_line bounds included); the
 * AdmissionReceipt / SessionHandle / SessionRef returns mirror the live
 * dataclass asdict shapes.
 *
 * Claimed siblings (ported in session_authority_flow.c / gateway_inbound_flow.c)
 * are SEAMED here, never re-ported: admission_row, self._receipt, self._handle,
 * self.authorize, self.logical_owner, self._require_admission_open,
 * self._current_owner, self.gate_refusal, self._runs_own_agent,
 * self._publish_pending, self._schedule, self._settle_cancelled,
 * self._commit_cancel, self._admitted, self._schedule_admitted,
 * self._release_refused_capture, self.physical_target. Ported targets compose
 * through real C calls (the oracle runs the extracted live methods, which
 * likewise compose): register->reserve/publish, publish->name_lane,
 * cancel_queued->receipt, cancel_or_claimed->cancel_queued,
 * resolve_unknown->receipt/resolve_unknown_as_owner,
 * resolve_unknown_as_owner->_resolved_unknown, interrupt->agent,
 * submit->_submit.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sau2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_session_authority_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc), ...} object;
 *   AuthorityRefusal adds reason/field/detail (field/detail _one_line-bound).
 * - Platform enum members travel as their .value strings; agents/transports as
 *   id strings (the oracle wraps non-empty ids in attribute doubles).
 * - logger calls are silent on both sides (not traced). authority_persistence
 *   is a silent handle lookup; its store's methods are the seams.
 * - asyncio.to_thread / tracked_write / track_mutation / asyncio.shield run
 *   their callable synchronously with no trace of their own (the inner store
 *   call IS traced; tracked_write then runs its `then` and `after` callbacks).
 *   locks (event_stream.lock, mutation_lock, claim_gate) are no-ops.
 * - sets (native_waiters, cancel_obligations) travel as SORTED id arrays on
 *   both sides; waiters as {aid: {"done": bool}}; the waiter.set_exception /
 *   waiter.set_result seams fire only when the popped waiter is not done.
 * - event_stream.execution is a state dict on the live session, mutated in
 *   place and emitted in out.state. A fresh LiveSession created by
 *   _publish_route / recover_native_sessions echoes the default field set.
 * - Scripted seams may carry {"__script__": true, "ret": v, "raise": spec};
 *   a bare array is a per-call queue (last value does NOT repeat — exhausted
 *   is fail-closed). A missing/exhausted script is a fail-closed RuntimeError.
 * - Fixture domain: admit_* seams return dicts; store.admissions.admit returns
 *   a [row, created] pair; check_native_route returns a [source, route] pair;
 *   prepare_result returns a [outcome, result] pair (CapturedResult(*prepared)).
 *   Integral floats are excluded (libjson cannot tell 1 from 1.0). ASCII only.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct sau2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sau2_ctx_t;

/* ── basic helpers ── */
static bool sau2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sau2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool sau2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *sau2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool sau2_str_eq(const json_t *v, const char *s) {
    const char *t = sau2_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* exceptions caught by `except Exception` (not the BaseException trio) */
static bool sau2_is_exception(const json_t *r) {
    if (!sau2_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0
        && strcmp(t, "GeneratorExit") != 0;
}

static json_t *sau2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* str(KeyError(k)) == repr(k) for the string ids the fixtures use. */
static json_t *sau2_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return sau2_err("KeyError", buf);
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces
 * (str.split()/join), other non-printables drop, bounded; empty -> None.
 * ASCII fixture strings: bytes >= 0x80 are treated as printable.
 */
static char *sau2_one_line(const char *value, size_t limit) {
    if (!value) return NULL;
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    bool pending_space = false, started = false;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (isspace(ch)) { if (started) pending_space = true; continue; }
        if (pending_space) { tmp[k++] = ' '; pending_space = false; }
        started = true;
        tmp[k++] = (char)ch;
    }
    tmp[k] = '\0';
    size_t w = 0;
    for (size_t i = 0; i < k; i++) {
        unsigned char ch = (unsigned char)tmp[i];
        if (ch < 0x20 || (ch >= 0x7f && ch <= 0x9f)) continue;
        tmp[w++] = tmp[i];
    }
    tmp[w] = '\0';
    if (w == 0) { free(tmp); return NULL; }
    if (w > limit) w = limit;
    char *out = malloc(w + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp, w);
    out[w] = '\0';
    free(tmp);
    return out;
}

/* The real AuthorityRefusal projection: reason/field/detail + message=reason. */
static json_t *sau2_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = sau2_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sau2_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *sau2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return sau2_refusal(json_get_str(spec, "reason", ""),
                            sau2_str(json_obj_get(spec, "field")),
                            sau2_str(json_obj_get(spec, "detail")));
    }
    const char *msg = json_get_str(spec, "message", "");
    return sau2_err(type, msg);
}

static char *sau2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* ── seam dispatch ── */
static json_t *sau2_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return sau2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *sau2_call(sau2_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = sau2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = sau2_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = sau2_entry_result(json_get(entry, idx));
        }
    } else {
        res = sau2_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* ── state helpers ── */
static json_t *sau2_sub(sau2_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *sau2_sub_ensure(sau2_ctx_t *c, const char *key) {
    json_t *v = sau2_sub(c, key);
    if (!v) { v = json_object(); json_set(c->state, key, v); }
    return v;
}

/* set-valued state (native_waiters, cancel_obligations) is a SORTED ARRAY. */
static json_t *sau2_set_ensure(sau2_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    if (!v || v->type != JSON_ARRAY) { v = json_array(); json_set(c->state, key, v); }
    return v;
}

/* dict.pop(key, None): owned copy out, key removed. */
static json_t *sau2_dict_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    if (!v) return NULL;
    json_t *copy = json_copy(v);
    json_obj_del(obj, key);
    return copy;
}

static bool sau2_dict_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

static bool sau2_set_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sau2_str_eq(json_get(arr, i), id)) return true;
    return false;
}

/* set.add: insert keeping sorted order (no dup) */
static void sau2_set_add(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    if (sau2_set_has(arr, id)) return;
    size_t n = json_len(arr), pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *s = sau2_str(json_get(arr, i));
        if (s && strcmp(s, id) > 0) { pos = i; break; }
    }
    json_t *copy = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == pos) json_append(copy, json_string(id));
        json_append(copy, json_copy(json_get(arr, i)));
    }
    if (pos == n) json_append(copy, json_string(id));
    for (size_t i = 0; i < arr->c.count; i++) {
        json_free(arr->c.items[i]);
        free(arr->c.keys ? arr->c.keys[i] : NULL);
    }
    free(arr->c.items); free(arr->c.keys);
    arr->c.items = copy->c.items; arr->c.keys = copy->c.keys; arr->c.count = copy->c.count;
    copy->c.items = NULL; copy->c.keys = NULL; copy->c.count = 0;
    json_free(copy);
}

static void sau2_set_discard(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        if (sau2_str_eq(json_get(arr, i), id)) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}

static bool sau2_caps_has(const json_t *caps, const char *cap) {
    if (!caps || caps->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(caps); i++)
        if (sau2_str_eq(json_get(caps, i), cap)) return true;
    return false;
}

static json_t *sau2_sorted_strs(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_copy(json_get(arr, i));
        size_t j = json_len(out);
        json_append(out, v);
        while (j > 0) {
            const char *a = sau2_str(json_get(out, j - 1));
            const char *b = sau2_str(json_get(out, j));
            if (a && b && strcmp(a, b) > 0) {
                json_t *t = out->c.items[j - 1];
                out->c.items[j - 1] = out->c.items[j];
                out->c.items[j] = t;
                j--;
            } else break;
        }
    }
    return out;
}

static json_t *sau2_live(sau2_ctx_t *c, const char *sid) {
    json_t *sessions = sau2_sub(c, "sessions");
    return (sessions && sid) ? json_obj_get(sessions, sid) : NULL;
}

static json_t *sau2_live_req(sau2_ctx_t *c, const char *sid, json_t **err) {
    json_t *live = sau2_live(c, sid);
    if (!live) *err = sau2_keyerror(sid);
    return live;
}

/* Propagate a raise result unless it is NULL (success). */
#define SAU2_CHK(v) do { if (sau2_is_raise(v)) return (v); } while (0)

static json_t *sau2_actor_arg(const json_t *actor) {
    json_t *a = json_object();
    if (!actor || actor->type != JSON_OBJECT) return a;
    json_t *subj = json_obj_get(actor, "subject");
    json_set(a, "subject", subj ? json_copy(subj) : json_null());
    json_t *pid = json_obj_get(actor, "profile_id");
    json_set(a, "profile_id", pid ? json_copy(pid) : json_null());
    json_set(a, "capabilities", sau2_sorted_strs(json_obj_get(actor, "capabilities")));
    json_t *tid = json_obj_get(actor, "transport_id");
    json_set(a, "transport_id", tid ? json_copy(tid) : json_null());
    return a;
}

static json_t *sau2_ref_arg(const json_t *ref) {
    json_t *r = json_object();
    json_t *pid = json_obj_get(ref, "profile_id");
    json_set(r, "profile_id", pid ? json_copy(pid) : json_null());
    json_t *sid = json_obj_get(ref, "session_id");
    json_set(r, "session_id", sid ? json_copy(sid) : json_null());
    return r;
}

static json_t *sau2_mkref(sau2_ctx_t *c, const char *sid) {
    json_t *r = json_object();
    json_t *pid = json_obj_get(c->state, "profile_id");
    json_set(r, "profile_id", pid ? json_copy(pid) : json_null());
    json_set_hdr(r, "session_id", sid ? sid : "");
    return r;
}

/* scalar equality for generation fences (None==None, int==int). */
static bool sau2_gen_eq(const json_t *a, const json_t *b) {
    if (sau2_is_null(a) && sau2_is_null(b)) return true;
    return a && b && a->type == JSON_NUMBER && b->type == JSON_NUMBER
        && a->num_val == b->num_val;
}

/* source projection for seam args: the raw dict (platform already .value). */
static json_t *sau2_source_arg(const json_t *source) {
    return source ? json_copy(source) : json_null();
}

/* ── forward declarations (ported targets compose through real C calls) ── */
static json_t *sau2_reserve_route(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_publish_route(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_register(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_name_lane(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_agent(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_admit_native_write(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_receipt(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_cancel_queued(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_cancel_or_claimed(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_resolve_unknown(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_resolve_unknown_as_owner(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_resolved_unknown(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_interrupt(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_refuse_head(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_recover_native_sessions(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_admit_automation(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_submit(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_submit_inner(sau2_ctx_t *c, const json_t *args);
static json_t *sau2_detached(sau2_ctx_t *c, const json_t *args);

/* ── claimed-sibling seam wrappers ── */
static json_t *sau2_admission_row(sau2_ctx_t *c, const json_t *record) {
    json_t *a = json_object();
    json_set(a, "record", record ? json_copy(record) : json_null());
    return sau2_call(c, "admission_row", a);
}
static json_t *sau2_receipt_sibling(sau2_ctx_t *c, const json_t *row) {
    json_t *a = json_object();
    json_set(a, "row", row ? json_copy(row) : json_null());
    return sau2_call(c, "self._receipt", a);
}
static json_t *sau2_handle(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self._handle", a);
}
static json_t *sau2_authorize(sau2_ctx_t *c, const json_t *actor, const json_t *ref,
                              const char *capability) {
    json_t *a = json_object();
    json_set(a, "actor", sau2_actor_arg(actor));
    json_set(a, "ref", sau2_ref_arg(ref));
    json_set_hdr(a, "capability", capability ? capability : "");
    return sau2_call(c, "self.authorize", a);
}
static json_t *sau2_logical_owner(sau2_ctx_t *c, const json_t *sid) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_copy(sid) : json_null());
    return sau2_call(c, "self.logical_owner", a);
}
static json_t *sau2_require_admission_open(sau2_ctx_t *c, bool queue_only) {
    json_t *a = json_object();
    json_set(a, "queue_only", json_bool(queue_only));
    return sau2_call(c, "self._require_admission_open", a);
}
static json_t *sau2_current_owner(sau2_ctx_t *c, const char *sid) {
    json_t *a = json_object();
    json_set_hdr(a, "session_id", sid ? sid : "");
    return sau2_call(c, "self._current_owner", a);
}
static json_t *sau2_gate_refusal(sau2_ctx_t *c, const json_t *check, bool queue_only) {
    json_t *a = json_object();
    json_set(a, "check", check ? json_copy(check) : json_null());
    json_set(a, "queue_only", json_bool(queue_only));
    return sau2_call(c, "self.gate_refusal", a);
}
static json_t *sau2_runs_own_agent(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self._runs_own_agent", a);
}
static json_t *sau2_publish_pending(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self._publish_pending", a);
}
static json_t *sau2_schedule(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self._schedule", a);
}
static json_t *sau2_settle_cancelled(sau2_ctx_t *c, const json_t *ref, const char *aid) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    json_set_hdr(a, "admission_id", aid ? aid : "");
    json_set_hdr(a, "outcome", "cancelled");
    return sau2_call(c, "self._settle_cancelled", a);
}
static json_t *sau2_commit_cancel(sau2_ctx_t *c, const json_t *ref, const char *aid,
                                  const json_t *before) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    json_set_hdr(a, "admission_id", aid ? aid : "");
    json_set(a, "before", before ? json_copy(before) : json_null());
    return sau2_call(c, "self._commit_cancel", a);
}
static json_t *sau2_admitted(sau2_ctx_t *c, const json_t *ref, const json_t *row) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    json_set(a, "event", json_null());
    json_set(a, "row", row ? json_copy(row) : json_null());
    return sau2_call(c, "self._admitted", a);
}
static json_t *sau2_schedule_admitted(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self._schedule_admitted", a);
}
static json_t *sau2_release_refused_capture(sau2_ctx_t *c, const json_t *request,
                                            const json_t *payload) {
    json_t *a = json_object();
    json_set(a, "request", request ? json_copy(request) : json_null());
    json_set(a, "payload", payload ? json_copy(payload) : json_null());
    return sau2_call(c, "self._release_refused_capture", a);
}
static json_t *sau2_physical_target(sau2_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau2_ref_arg(ref));
    return sau2_call(c, "self.physical_target", a);
}

/* detached_route_of(self.source) is not None — the LiveSession.detached body. */
static json_t *sau2_detached_route_of(sau2_ctx_t *c, const json_t *source) {
    json_t *a = json_object();
    json_set(a, "source", sau2_source_arg(source));
    return sau2_call(c, "detached_route_of", a);
}

/* A fresh LiveSession(source, route) as the default state-entry dict. */
static json_t *sau2_fresh_live(const json_t *source, const char *route, const char *sid) {
    json_t *l = json_object();
    json_set_hdr(l, "route", route ? route : "");
    json_set(l, "source", sau2_source_arg(source));
    json_set(l, "settling", json_object());
    json_set(l, "subscribers", json_object());
    json_set(l, "task", json_null());
    json_set(l, "rescan", json_bool(false));
    json_set(l, "background_wait", json_null());
    json_set(l, "pause_notified", json_bool(false));
    json_t *es = json_object();
    json_set(es, "execution", json_object());
    json_set(l, "event_stream", es);
    json_set_hdr(l, "token", sid ? sid : "");
    return l;
}

/* ══════════════ routing / register flows ══════════════ */

/* PoP: sau2_detached @ gateway/session_authority.py:detached */
static json_t *sau2_detached(sau2_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);
    if (err) return err;
    json_t *r = sau2_detached_route_of(c, json_obj_get(live, "source"));
    SAU2_CHK(r);
    bool out = !sau2_is_null(r);
    json_free(r);
    return json_bool(out);
}

/* PoP: sau2_reserve_route @ gateway/session_authority.py:_reserve_route */
static json_t *sau2_reserve_route(sau2_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    json_t *ro = sau2_require_admission_open(c, false);
    SAU2_CHK(ro);
    json_free(ro);
    json_t *ga = json_object();
    json_set(ga, "source", sau2_source_arg(source));
    json_t *entry = sau2_call(c, "runner.session_store.get_or_create_session", ga);
    SAU2_CHK(entry);
    if (sau2_is_null(entry)) {
        json_free(entry);
        return sau2_err("AttributeError", "'NoneType' object has no attribute 'session_id'");
    }
    json_t *esid = json_obj_get(entry, "session_id");
    json_t *lo = sau2_logical_owner(c, esid);
    SAU2_CHK(lo);
    const char *sid = sau2_str(lo);
    json_t *geta = json_object();
    json_set_hdr(geta, "session_id", sid ? sid : "");
    json_t *row = sau2_call(c, "store.sessions.get", geta);
    if (sau2_is_raise(row)) { json_free(entry); json_free(lo); return row; }
    bool absent = sau2_is_null(row);
    json_free(row);
    if (absent) {
        json_t *platform = source ? json_obj_get(source, "platform") : NULL;
        json_t *ca = json_object();
        json_set_hdr(ca, "session_id", sid ? sid : "");
        json_set(ca, "source", platform ? json_copy(platform) : json_null());
        json_t *cr = sau2_call(c, "store.sessions.create", ca);
        if (sau2_is_raise(cr)) { json_free(entry); json_free(lo); return cr; }
        json_free(cr);
    }
    json_t *out = json_array();
    json_append(out, lo);  /* owned sid */
    json_t *skey = json_obj_get(entry, "session_key");
    json_append(out, skey ? json_copy(skey) : json_null());
    json_free(entry);
    return out;
}

/* PoP: sau2_name_lane @ gateway/session_authority.py:name_lane */
static json_t *sau2_name_lane(sau2_ctx_t *c, const json_t *args) {
    const char *route = json_get_str(args, "route", "");
    const char *sid = json_get_str(args, "session_id", "");
    json_t *source = json_obj_get(args, "source");
    json_t *sessions = sau2_sub(c, "sessions");
    if (sessions) {
        for (size_t i = 0; i < sessions->c.count; i++) {
            const char *other_sid = sessions->c.keys[i];
            json_t *other = sessions->c.items[i];
            if (strcmp(other_sid, sid) == 0) continue;
            if (sau2_str_eq(json_obj_get(other, "route"), route)) {
                json_t *da = json_object();
                json_set_hdr(da, "session_id", other_sid);
                json_t *oroute = json_obj_get(other, "route");
                json_set(da, "route", oroute ? json_copy(oroute) : json_null());
                json_t *dr = sau2_call(c, "detach", da);
                SAU2_CHK(dr);
                json_free(dr);
            }
        }
    }
    json_t *live = sau2_live(c, sid);
    if (live && !sau2_str_eq(json_obj_get(live, "route"), route)) {
        json_t *dr = sau2_detached_route_of(c, json_obj_get(live, "source"));
        SAU2_CHK(dr);
        bool is_detached = !sau2_is_null(dr);
        json_free(dr);
        json_t *left = NULL;
        if (is_detached) {
            json_t *lr = json_obj_get(live, "route");
            left = lr ? json_copy(lr) : json_null();
        }
        json_set(live, "source", sau2_source_arg(source));
        json_set_hdr(live, "route", route);
        if (left) {
            json_t *fa = json_object();
            json_set(fa, "route", left);  /* owned */
            json_t *fr = sau2_call(c, "forget_detached", fa);
            SAU2_CHK(fr);
            json_free(fr);
        }
    }
    return NULL;
}

/* PoP: sau2_publish_route @ gateway/session_authority.py:_publish_route */
static json_t *sau2_publish_route(sau2_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *sid = json_get_str(args, "session_id", "");
    const char *route = json_get_str(args, "route", "");
    json_t *ro = sau2_require_admission_open(c, false);
    SAU2_CHK(ro);
    json_free(ro);
    if (!sau2_live(c, sid)) {
        json_t *sessions = sau2_sub_ensure(c, "sessions");
        json_set(sessions, sid, sau2_fresh_live(source, route, sid));
    }
    json_t *na = json_object();
    json_set_hdr(na, "route", route);
    json_set_hdr(na, "session_id", sid);
    json_set(na, "source", sau2_source_arg(source));
    json_t *nr = sau2_name_lane(c, na);
    json_free(na);
    SAU2_CHK(nr);
    json_free(nr);
    return sau2_mkref(c, sid);
}

/* PoP: sau2_register @ gateway/session_authority.py:register */
static json_t *sau2_register(sau2_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    json_t *ra = json_object();
    json_set(ra, "source", sau2_source_arg(source));
    json_t *rr = sau2_reserve_route(c, ra);
    json_free(ra);
    SAU2_CHK(rr);
    /* _publish_route(source, *reserved) — reserved is [sid, route]. */
    json_t *sid = json_get(rr, 0);
    json_t *route = json_get(rr, 1);
    json_t *pa = json_object();
    json_set(pa, "source", sau2_source_arg(source));
    json_set(pa, "session_id", sid ? json_copy(sid) : json_null());
    json_set(pa, "route", route ? json_copy(route) : json_null());
    json_free(rr);
    json_t *pr = sau2_publish_route(c, pa);
    json_free(pa);
    return pr;
}

/* PoP: sau2_agent @ gateway/session_authority.py:agent */
static json_t *sau2_agent(sau2_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);
    if (err) return err;
    json_t *ka = json_object();
    json_t *lr = json_obj_get(live, "route");
    json_set(ka, "route", lr ? json_copy(lr) : json_null());
    json_set_hdr(ka, "session_id", sid);
    json_t *key = sau2_call(c, "running_key", ka);
    SAU2_CHK(key);
    json_t *runner = sau2_sub(c, "runner");
    json_t *resident = runner ? json_obj_get(runner, "_resident_agent_for") : NULL;
    const char *seam = sau2_truthy(resident) ? "runner._resident_agent_for"
                                             : "runner._cached_agent_for";
    json_t *la = json_object();
    json_set(la, "key", key);  /* owned */
    return sau2_call(c, seam, la);
}

/* ══════════════ native admission write ══════════════ */

/* PoP: sau2_admit_native_write @ gateway/session_authority.py:_admit_native_write */
static json_t *sau2_admit_native_write(sau2_ctx_t *c, const json_t *args) {
    const char *principal_id = json_get_str(args, "principal_id", "");
    const char *session_id = json_get_str(args, "session_id", "");
    const char *request_id = json_get_str(args, "request_id", "");
    json_t *payload = json_obj_get(args, "payload");
    json_t *fence = json_obj_get(args, "fence");
    json_t *rewrite = json_obj_get(args, "rewrite");
    json_t *raw = json_obj_get(args, "raw");
    /* fresh = payload.get('native_text_v1', {}).get('media', []) */
    json_t *ntv = payload ? json_obj_get(payload, "native_text_v1") : NULL;
    json_t *fresh;
    if (!ntv) {
        fresh = json_array();  /* default {} -> .get('media', []) -> [] */
    } else if (ntv->type != JSON_OBJECT) {
        return sau2_err("AttributeError", "'NoneType' object has no attribute 'get'");
    } else {
        json_t *m = json_obj_get(ntv, "media");
        fresh = m ? json_copy(m) : json_array();
    }
    json_t *co = sau2_current_owner(c, session_id);
    SAU2_CHK(co);
    const char *sid = sau2_str(co);
    json_free(co);
    json_t *err = NULL;  /* the try-block exception, if any */
    json_t *result = NULL;
    /* try: */
    json_t *ra = json_object();
    json_set_hdr(ra, "principal_id", principal_id);
    json_set_hdr(ra, "session_id", sid ? sid : "");
    json_set_hdr(ra, "request_id", request_id);
    json_set(ra, "payload", payload ? json_copy(payload) : json_null());
    json_set(ra, "raw", raw ? json_copy(raw) : json_null());
    json_t *payload2 = sau2_call(c, "reconcile_native_retry", ra);
    if (sau2_is_raise(payload2)) { err = payload2; goto cleanup; }
    json_t *gate = sau2_gate_refusal(c, fence, false);
    if (sau2_is_raise(gate)) { json_free(payload2); err = gate; goto cleanup; }
    json_t *rewrite_val = (rewrite && rewrite->type != JSON_NULL)
        ? json_obj_get(rewrite, "rewrite") : NULL;
    json_t *aa = json_object();
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(aa, "epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(aa, "principal_id", principal_id);
    json_set_hdr(aa, "session_id", sid ? sid : "");
    json_set_hdr(aa, "request_id", request_id);
    json_set(aa, "payload", payload2);  /* owned */
    json_set(aa, "gate", gate);  /* owned */
    json_set(aa, "rewrite", rewrite_val ? json_copy(rewrite_val) : json_null());
    json_t *admit = sau2_call(c, "store.admissions.admit", aa);
    if (sau2_is_raise(admit)) { err = admit; goto cleanup; }
    json_t *row = json_get(admit, 0);
    /* rewrite.applied = created is a write on the external arg object: not
     * part of authority state, never echoed (the oracle double is discarded). */
    json_t *ar = sau2_admission_row(c, row);
    json_free(admit);
    if (sau2_is_raise(ar)) { err = ar; goto cleanup; }
    result = ar;
cleanup:
    if (err) {
        if (sau2_is_exception(err)) {
            json_t *dcl = sau2_call(c, "drop_capture_lease", json_object());
            if (sau2_is_raise(dcl)) { json_free(err); json_free(fresh); return dcl; }
            json_free(dcl);
            json_t *ua = json_object();
            json_set(ua, "fresh", fresh);  /* owned */
            json_set(ua, "retain_history", json_bool(false));
            json_t *rum = sau2_call(c, "release_unheld_media", ua);
            if (sau2_is_raise(rum)) { json_free(err); return rum; }
            json_free(rum);
            return err;  /* re-raise the original */
        }
        json_free(fresh);
        return err;  /* BaseException propagates without cleanup */
    }
    json_free(fresh);
    return result;
}

/* ══════════════ receipt / cancel / resolve-unknown chains ══════════════ */

/* PoP: sau2_receipt @ gateway/session_authority.py:receipt */
static json_t *sau2_receipt(sau2_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *ar = sau2_authorize(c, actor, ref, "session:submit");
    SAU2_CHK(ar);
    json_free(ar);
    json_t *ga = json_object();
    json_set_hdr(ga, "admission_id", aid);
    json_t *record = sau2_call(c, "store.admissions.get", ga);
    SAU2_CHK(record);
    json_t *row = sau2_admission_row(c, record);
    json_free(record);
    SAU2_CHK(row);
    if (sau2_is_null(row) || !sau2_str_eq(json_obj_get(row, "target_session_id"), sid)) {
        json_free(row);
        return sau2_refusal("not_found", NULL, NULL);
    }
    json_t *soa = json_object();
    json_t *prid = json_obj_get(row, "principal_id");
    json_set(soa, "a", prid ? json_copy(prid) : json_null());
    json_t *subj = actor ? json_obj_get(actor, "subject") : NULL;
    json_set(soa, "b", subj ? json_copy(subj) : json_null());
    json_t *so = sau2_call(c, "same_owner", soa);
    if (sau2_is_raise(so)) { json_free(row); return so; }
    bool same = sau2_truthy(so);
    json_free(so);
    if (!same && !sau2_caps_has(actor ? json_obj_get(actor, "capabilities") : NULL,
                                "session:control")) {
        json_free(row);
        return sau2_refusal("permission_denied", NULL, NULL);
    }
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);
    if (err) { json_free(row); return err; }
    json_t *settling = json_obj_get(live, "settling");
    json_t *masked = (settling && settling->type == JSON_OBJECT)
        ? json_obj_get(settling, aid) : NULL;
    json_t *final_row = masked ? json_copy(masked) : row;  /* owned */
    if (masked) json_free(row);
    return sau2_receipt_sibling(c, final_row);
}

/* PoP: sau2_cancel_queued @ gateway/session_authority.py:cancel_queued */
static json_t *sau2_cancel_queued(sau2_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *before = sau2_receipt(c, args);
    SAU2_CHK(before);
    /* track_mutation / shield: synchronous passthrough, no trace of their own. */
    json_t *row = sau2_commit_cancel(c, ref, aid, before);
    json_free(before);
    SAU2_CHK(row);
    json_t *ra = json_object();
    json_set_hdr(ra, "admission_id", aid);
    json_t *rm = sau2_call(c, "release_admission_media", ra);
    if (sau2_is_raise(rm)) { json_free(row); return rm; }
    json_free(rm);
    return sau2_receipt_sibling(c, row);
}

/* PoP: sau2_cancel_or_claimed @ gateway/session_authority.py:cancel_or_claimed */
static json_t *sau2_cancel_or_claimed(sau2_ctx_t *c, const json_t *args) {
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *cq = sau2_cancel_queued(c, args);
    if (sau2_is_raise(cq)) {
        bool stale = sau2_str_eq(json_obj_get(cq, "__raise__"), "AuthorityRefusal")
            && sau2_str_eq(json_obj_get(cq, "reason"), "stale_generation");
        if (!stale) return cq;
        json_free(cq);  /* swallow stale_generation, fall through */
    } else {
        json_free(cq);
        return json_null();  /* cancelled */
    }
    json_t *ga = json_object();
    json_set_hdr(ga, "admission_id", aid);
    json_t *record = sau2_call(c, "store.admissions.get", ga);
    SAU2_CHK(record);
    json_t *row = sau2_admission_row(c, record);
    json_free(record);
    SAU2_CHK(row);
    if (sau2_is_null(row) || !sau2_str_eq(json_obj_get(row, "status"), "started")) {
        json_free(row);
        return sau2_refusal("stale_generation", NULL, NULL);
    }
    return row;
}

/* PoP: sau2_resolve_unknown @ gateway/session_authority.py:resolve_unknown */
static json_t *sau2_resolve_unknown(sau2_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    json_t *ar = sau2_authorize(c, actor, ref, "session:control");
    SAU2_CHK(ar);
    json_free(ar);
    json_t *rc = sau2_receipt(c, args);
    SAU2_CHK(rc);
    json_free(rc);
    json_t *oa = json_object();
    json_set(oa, "ref", sau2_ref_arg(ref));
    json_t *aid = json_obj_get(args, "admission_id");
    json_set(oa, "admission_id", aid ? json_copy(aid) : json_null());
    json_t *gen = json_obj_get(args, "generation");
    json_set(oa, "generation", gen ? json_copy(gen) : json_null());
    json_set(oa, "notice", json_null());
    json_t *or_ = sau2_resolve_unknown_as_owner(c, oa);
    json_free(oa);
    return or_;
}

/* PoP: sau2_resolve_unknown_as_owner @ gateway/session_authority.py:resolve_unknown_as_owner */
static json_t *sau2_resolve_unknown_as_owner(sau2_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *gen = json_obj_get(args, "generation");
    json_t *notice = json_obj_get(args, "notice");
    json_t *pr = sau2_sub(c, "pending_results");
    json_t *captured = pr ? json_obj_get(pr, aid) : NULL;  /* borrowed */
    json_t *ga = json_object();
    json_set_hdr(ga, "admission_id", aid);
    json_t *before = sau2_call(c, "store.admissions.get", ga);
    SAU2_CHK(before);
    json_t *prepared = NULL;  /* owned */
    bool have_captured = captured && captured->type != JSON_NULL;
    bool before_unknown = !sau2_is_null(before)
        && sau2_str_eq(json_obj_get(before, "status"), "unknown");
    json_free(before);
    if (have_captured && before_unknown) {
        json_t *ga2 = json_object();
        json_set_hdr(ga2, "admission_id", aid);
        json_t *record2 = sau2_call(c, "store.admissions.get", ga2);
        SAU2_CHK(record2);
        json_t *row = sau2_admission_row(c, record2);
        json_free(record2);
        SAU2_CHK(row);
        /* captured['result'].get('final_response') or '' */
        json_t *cresult = json_obj_get(captured, "result");
        json_t *fr = (cresult && cresult->type == JSON_OBJECT)
            ? json_obj_get(cresult, "final_response") : NULL;
        const char *fr_s = sau2_truthy(fr) ? sau2_str(fr) : "";
        json_t *pa = json_object();
        json_set(pa, "row", row);  /* owned */
        json_set_hdr(pa, "final_response", fr_s ? fr_s : "");
        json_set_hdr(pa, "status", "completed");
        json_set(pa, "captured", json_copy(captured));
        prepared = sau2_call(c, "prepare_result", pa);
        SAU2_CHK(prepared);
    }
    /* captured=None if prepared is None else CapturedResult(*prepared) */
    json_t *captured_arg = json_null();
    if (prepared && prepared->type != JSON_NULL) {
        json_free(captured_arg);
        captured_arg = json_object();
        json_t *o = json_get(prepared, 0);
        json_t *r = json_get(prepared, 1);
        json_set(captured_arg, "outcome", o ? json_copy(o) : json_null());
        json_set(captured_arg, "result", r ? json_copy(r) : json_null());
    }
    /* tracked_write(partial(resolve_unknown, ...), then=_resolved_unknown) */
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_t *ua = json_object();
    json_set(ua, "epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(ua, "admission_id", aid);
    json_set(ua, "generation", gen ? json_copy(gen) : json_null());
    json_set(ua, "captured", captured_arg);  /* owned */
    json_set(ua, "notice", notice ? json_copy(notice) : json_null());
    json_t *record3 = sau2_call(c, "store.admissions.resolve_unknown", ua);
    if (sau2_is_raise(record3)) { json_free(prepared); return record3; }
    json_t *ta = json_object();
    json_set(ta, "ref", sau2_ref_arg(ref));
    json_set_hdr(ta, "admission_id", aid);
    json_set(ta, "prepared", prepared ? prepared : json_null());  /* owned */
    json_set(ta, "captured", captured ? json_copy(captured) : json_null());
    json_set(ta, "record", record3);  /* owned */
    return sau2_resolved_unknown(c, ta);
}

/* PoP: sau2_resolved_unknown @ gateway/session_authority.py:_resolved_unknown */
static json_t *sau2_resolved_unknown(sau2_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *prepared = json_obj_get(args, "prepared");
    json_t *captured = json_obj_get(args, "captured");
    json_t *record = json_obj_get(args, "record");
    json_t *row = sau2_admission_row(c, record);
    SAU2_CHK(row);
    json_t *pr = sau2_sub(c, "pending_results");
    if (pr) json_obj_del(pr, aid);
    json_t *live = sau2_live(c, sid);
    if (!live) {
        json_t *ra = json_object();
        json_set_hdr(ra, "session_id", sid);
        json_t *rr = sau2_call(c, "restore_retired", ra);
        if (sau2_is_raise(rr)) { json_free(row); return rr; }
        json_free(rr);
        live = sau2_live(c, sid);
    }
    json_t *es = live ? json_obj_get(live, "event_stream") : NULL;
    json_t *execution = (es && es->type == JSON_OBJECT) ? json_obj_get(es, "execution") : NULL;
    json_t *exec_aid = (execution && execution->type == JSON_OBJECT)
        ? json_obj_get(execution, "admission_id") : NULL;
    bool drain_owns = live && sau2_str_eq(exec_aid, aid);
    json_t *epoch = json_obj_get(c->state, "epoch");
    bool epoch_match = sau2_gen_eq(json_obj_get(row, "owner_epoch"), epoch);
    bool have_prepared = prepared && prepared->type != JSON_NULL;
    if (!drain_owns) {
        json_t *try_err = NULL;
        /* try: */
        if (have_prepared && epoch_match) {
            json_t *err = NULL;
            json_t *live2 = sau2_live_req(c, sid, &err);
            if (err) { try_err = err; }
            else {
                (void)live2;
                json_t *cresult = captured ? json_obj_get(captured, "result") : NULL;
                json_t *fr = (cresult && cresult->type == JSON_OBJECT)
                    ? json_obj_get(cresult, "final_response") : NULL;
                const char *fr_s = sau2_truthy(fr) ? sau2_str(fr) : "";
                json_t *ca = json_object();
                json_set(ca, "ref", sau2_ref_arg(ref));
                json_set(ca, "row", json_copy(row));
                json_set_hdr(ca, "final_response", fr_s ? fr_s : "");
                json_set(ca, "captured", captured ? json_copy(captured) : json_null());
                json_t *pc = sau2_call(c, "_publish_completion", ca);
                if (sau2_is_raise(pc)) try_err = pc; else json_free(pc);
            }
        }
        if (!try_err) {
            json_t *da = json_object();
            json_set_hdr(da, "admission_id", aid);
            json_set(da, "owed", json_bool(have_prepared && epoch_match));
            json_t *dr = sau2_call(c, "deliver_resolved", da);
            if (sau2_is_raise(dr)) try_err = dr; else json_free(dr);
        }
        /* finally: release_media_after_delivery(self, row) */
        json_t *fa = json_object();
        json_set(fa, "row", json_copy(row));
        json_t *fm = sau2_call(c, "release_media_after_delivery", fa);
        if (sau2_is_raise(fm)) { json_free(row); if (try_err) json_free(try_err); return fm; }
        json_free(fm);
        if (try_err) { json_free(row); return try_err; }
    } else {
        json_t *ha = json_object();
        json_set_hdr(ha, "admission_id", aid);
        json_t *hc = sau2_call(c, "hand_collection_to_drain", ha);
        if (sau2_is_raise(hc)) { json_free(row); return hc; }
        json_free(hc);
    }
    json_t *pp = sau2_publish_pending(c, ref);
    if (sau2_is_raise(pp)) { json_free(row); return pp; }
    json_free(pp);
    json_t *sc = sau2_schedule(c, ref);
    if (sau2_is_raise(sc)) { json_free(row); return sc; }
    json_free(sc);
    return sau2_receipt_sibling(c, row);
}

/* ══════════════ interrupt / head refusal ══════════════ */

/* PoP: sau2_interrupt @ gateway/session_authority.py:interrupt */
static json_t *sau2_interrupt(sau2_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *gen = json_obj_get(args, "generation");
    json_t *ar = sau2_authorize(c, actor, ref, "session:control");
    SAU2_CHK(ar);
    json_free(ar);
    json_t *handle = sau2_handle(c, ref);
    SAU2_CHK(handle);
    if (!sau2_gen_eq(json_obj_get(handle, "execution_generation"), gen)) {
        json_free(handle);
        return sau2_refusal("stale_generation", NULL, NULL);
    }
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);
    if (err) { json_free(handle); return err; }
    json_t *agent = NULL;  /* owned id node or NULL */
    bool running = sau2_str_eq(json_obj_get(handle, "execution_state"), "running");
    json_free(handle);
    if (running) {
        json_t *adopted = sau2_sub(c, "adopted");
        json_t *pair = adopted ? json_obj_get(adopted, sid) : NULL;
        json_t *adopted_gen = (pair && pair->type == JSON_ARRAY && json_len(pair) > 0)
            ? json_get(pair, 0) : NULL;
        if (pair && pair->type == JSON_ARRAY && json_len(pair) > 1) {
            json_t *a1 = json_get(pair, 1);
            agent = (a1 && a1->type != JSON_NULL) ? json_copy(a1) : NULL;
        }
        if (!sau2_gen_eq(adopted_gen, gen)) {
            if (agent) { json_free(agent); agent = NULL; }
            json_t *es = json_obj_get(live, "event_stream");
            json_t *execution = (es && es->type == JSON_OBJECT) ? json_obj_get(es, "execution") : NULL;
            json_t *exec_gen = (execution && execution->type == JSON_OBJECT)
                ? json_obj_get(execution, "execution_generation") : NULL;
            json_t *executing = sau2_sub(c, "executing");
            json_t *cur_gen = executing ? json_obj_get(executing, sid) : NULL;
            bool returned = sau2_gen_eq(exec_gen, gen) && !sau2_gen_eq(cur_gen, gen);
            if (!returned) {
                json_t *aa = json_object();
                json_set(aa, "ref", sau2_ref_arg(ref));
                agent = sau2_agent(c, aa);
                json_free(aa);
                SAU2_CHK(agent);
                json_t *ra = json_object();
                json_set(ra, "ref", sau2_ref_arg(ref));
                json_t *roa = sau2_runs_own_agent(c, ra);
                json_free(ra);
                if (sau2_is_raise(roa)) { if (agent) json_free(agent); return roa; }
                bool own = sau2_truthy(roa);
                json_free(roa);
                if (!own) {
                    json_t *pst = sau2_sub_ensure(c, "pending_stops");
                    json_set(pst, sid, gen ? json_copy(gen) : json_null());
                }
            }
        }
        if (agent && agent->type != JSON_NULL) {
            json_t *dst = sau2_sub_ensure(c, "delivered_stops");
            json_set(dst, sid, gen ? json_copy(gen) : json_null());
        }
    }
    json_t *wa = json_object();
    json_set(wa, "ref", sau2_ref_arg(ref));
    json_set(wa, "person_input", json_bool(true));
    json_t *wb = sau2_call(c, "wake_background_wait", wa);
    if (sau2_is_raise(wb)) { if (agent) json_free(agent); return wb; }
    json_free(wb);
    if (agent && agent->type != JSON_NULL) {
        json_t *ia = json_object();
        json_set(ia, "agent", json_copy(agent));
        json_t *ir = sau2_call(c, "request_hard_interrupt", ia);
        if (sau2_is_raise(ir)) { json_free(agent); return ir; }
        json_free(ir);
    }
    if (agent) json_free(agent);
    return sau2_handle(c, ref);
}

/* PoP: sau2_refuse_head @ gateway/session_authority.py:_refuse_head */
static json_t *sau2_refuse_head(sau2_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *aid = json_get_str(args, "admission_id", "");
    const char *reason = json_get_str(args, "reason", "");
    json_t *ga = json_object();
    json_set_hdr(ga, "admission_id", aid);
    json_t *before = sau2_call(c, "store.admissions.get", ga);
    SAU2_CHK(before);
    json_t *co = sau2_set_ensure(c, "cancel_obligations");
    if (!sau2_is_null(before) && sau2_str_eq(json_obj_get(before, "status"), "queued"))
        sau2_set_add(co, aid);
    json_free(before);
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_t *ca = json_object();
    json_set(ca, "epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(ca, "admission_id", aid);
    json_t *row = sau2_call(c, "store.admissions.cancel", ca);
    if (sau2_is_raise(row)) {
        if (sau2_str_eq(json_obj_get(row, "__raise__"), "AuthorityRefusal")) {
            sau2_set_discard(co, aid);
            json_free(row);
            return json_bool(false);
        }
        return row;
    }
    if (sau2_is_null(row)) {
        json_free(row);
        return sau2_err("AttributeError", "'NoneType' object has no attribute 'status'");
    }
    const char *status = json_get_str(row, "status", "");
    const char *outcome = json_get_str(row, "outcome", "");
    if (strcmp(status, "terminal") != 0 || strcmp(outcome, "cancelled") != 0) {
        sau2_set_discard(co, aid);
        json_free(row);
        return json_bool(false);
    }
    json_free(row);
    if (sau2_set_has(co, aid)) {
        json_t *waiters = sau2_sub_ensure(c, "waiters");
        json_t *waiter = sau2_dict_pop(waiters, aid);
        if (waiter) {
            bool wdone = json_get_bool(waiter, "done", false);
            json_free(waiter);
            if (!wdone) {
                json_t *wa = json_object();
                json_set_hdr(wa, "admission_id", aid);
                json_set_hdr(wa, "reason", reason);
                json_t *wr = sau2_call(c, "waiter.set_exception", wa);
                SAU2_CHK(wr);
                json_free(wr);
            }
        }
        json_t *sr = sau2_settle_cancelled(c, ref, aid);
        SAU2_CHK(sr);
        json_free(sr);
    }
    json_t *ra = json_object();
    json_set_hdr(ra, "admission_id", aid);
    json_t *rm = sau2_call(c, "release_admission_media", ra);
    SAU2_CHK(rm);
    json_free(rm);
    return json_bool(true);
}

/* ══════════════ native recovery / automation / submit ══════════════ */

/* PoP: sau2_recover_native_sessions @ gateway/session_authority.py:recover_native_sessions */
static json_t *sau2_recover_native_sessions(sau2_ctx_t *c, const json_t *args) {
    json_t *bindings = json_obj_get(args, "bindings");
    json_t *results = json_object();
    if (!bindings || bindings->type != JSON_ARRAY) return results;
    size_t n = json_len(bindings);
    for (size_t i = 0; i < n; i++) {
        json_t *b = json_get(bindings, i);
        json_t *sidn = json_get(b, 0);
        const char *sid = sau2_str(sidn);
        json_t *available_source = json_get(b, 1);
        json_t *adapter = json_get(b, 2);
        /* count occurrences of sid */
        size_t count = 0;
        for (size_t j = 0; j < n; j++)
            if (sau2_str_eq(json_get(json_get(bindings, j), 0), sid)) count++;
        json_t *err = NULL;  /* AuthorityRefusal caught -> results[sid]=reason */
        json_t *prop = NULL; /* other raise -> propagate out of the function */
        if (count != 1) { err = sau2_refusal("admission_conflict", NULL, NULL); goto settle; }
        json_t *la = json_object();
        json_set_hdr(la, "session_id", sid ? sid : "");
        json_set(la, "pending_only", json_bool(false));
        json_t *listed = sau2_call(c, "store.admissions.list", la);
        if (sau2_is_raise(listed)) {
            if (sau2_str_eq(json_obj_get(listed, "__raise__"), "AuthorityRefusal")) err = listed;
            else prop = listed;
            goto settle;
        }
        /* rows = [admission_row(row) for row in listed] */
        json_t *rows = json_array();
        for (size_t k = 0; k < json_len(listed); k++) {
            json_t *row = sau2_admission_row(c, json_get(listed, k));
            if (sau2_is_raise(row)) {
                if (sau2_str_eq(json_obj_get(row, "__raise__"), "AuthorityRefusal")) err = row;
                else prop = row;
                json_free(listed); json_free(rows); goto settle;
            }
            json_append(rows, row);
        }
        json_free(listed);
        /* native = [row for row in rows if 'native_text_v1' in row['payload']] */
        json_t *native_last = NULL;
        bool any_native = false;
        for (size_t k = 0; k < json_len(rows); k++) {
            json_t *row = json_get(rows, k);
            json_t *payload = json_obj_get(row, "payload");
            if (sau2_dict_has(payload, "native_text_v1")) {
                any_native = true;
                native_last = row;
            }
        }
        if (!any_native) { json_free(rows); err = sau2_refusal("not_found", NULL, NULL); goto settle; }
        json_t *ref = sau2_mkref(c, sid);
        json_t *target = sau2_physical_target(c, ref);
        if (sau2_is_raise(target)) {
            if (sau2_str_eq(json_obj_get(target, "__raise__"), "AuthorityRefusal")) err = target;
            else prop = target;
            json_free(ref); json_free(rows); goto settle;
        }
        /* source, route = check_native_route(runner, native[-1].payload, target, ...) */
        json_t *cna = json_object();
        json_set(cna, "payload", json_copy(json_obj_get(native_last, "payload")));
        json_set(cna, "target", json_copy(target));
        json_set(cna, "available_source", sau2_source_arg(available_source));
        json_set(cna, "adapter", adapter ? json_copy(adapter) : json_null());
        json_t *cnr = sau2_call(c, "check_native_route", cna);
        if (sau2_is_raise(cnr)) {
            if (sau2_str_eq(json_obj_get(cnr, "__raise__"), "AuthorityRefusal")) err = cnr;
            else prop = cnr;
            json_free(ref); json_free(rows); json_free(target); goto settle;
        }
        json_t *source = json_get(cnr, 0);
        json_t *route = json_get(cnr, 1);
        for (size_t k = 0; k < json_len(rows); k++) {
            json_t *row = json_get(rows, k);
            if (!sau2_str_eq(json_obj_get(row, "status"), "queued")) continue;
            json_t *payload = json_obj_get(row, "payload");
            if (sau2_dict_has(payload, "native_text_v1")) {
                json_t *a = json_object();
                json_set(a, "payload", json_copy(payload));
                json_set(a, "target", json_copy(target));
                json_set(a, "available_source", sau2_source_arg(available_source));
                json_set(a, "adapter", adapter ? json_copy(adapter) : json_null());
                json_t *r = sau2_call(c, "check_native_route", a);
                if (sau2_is_raise(r)) {
                    if (sau2_str_eq(json_obj_get(r, "__raise__"), "AuthorityRefusal")) err = r;
                    else prop = r;
                    json_free(cnr); json_free(ref); json_free(rows); json_free(target); goto settle;
                }
                json_free(r);
            } else if (sau2_dict_has(payload, "local_automation_v1")) {
                json_t *a = json_object();
                json_set(a, "ref", sau2_ref_arg(ref));
                json_set(a, "row", json_copy(row));
                json_set(a, "route", route ? json_copy(route) : json_null());
                json_t *r = sau2_call(c, "check_local_automation", a);
                if (sau2_is_raise(r)) {
                    if (sau2_str_eq(json_obj_get(r, "__raise__"), "AuthorityRefusal")) err = r;
                    else prop = r;
                    json_free(cnr); json_free(ref); json_free(rows); json_free(target); goto settle;
                }
                json_free(r);
            } else {
                json_t *a = json_object();
                json_set(a, "ref", sau2_ref_arg(ref));
                json_set(a, "row", json_copy(row));
                json_set(a, "source", sau2_source_arg(source));
                json_t *r = sau2_call(c, "check_local_input", a);
                if (sau2_is_raise(r)) {
                    if (sau2_str_eq(json_obj_get(r, "__raise__"), "AuthorityRefusal")) err = r;
                    else prop = r;
                    json_free(cnr); json_free(ref); json_free(rows); json_free(target); goto settle;
                }
                json_free(r);
            }
        }
        json_t *ro = sau2_require_admission_open(c, false);
        if (sau2_is_raise(ro)) {
            if (sau2_str_eq(json_obj_get(ro, "__raise__"), "AuthorityRefusal")) err = ro;
            else prop = ro;
            json_free(cnr); json_free(ref); json_free(rows); json_free(target); goto settle;
        }
        json_free(ro);
        if (!sau2_live(c, sid)) {
            json_t *sessions = sau2_sub_ensure(c, "sessions");
            json_set(sessions, sid, sau2_fresh_live(source, sau2_str(route), sid));
        }
        bool any_unknown = false;
        for (size_t k = 0; k < json_len(rows); k++)
            if (sau2_str_eq(json_obj_get(json_get(rows, k), "status"), "unknown")) any_unknown = true;
        if (any_unknown) {
            json_free(cnr); json_free(ref); json_free(rows); json_free(target);
            err = sau2_refusal("unknown_execution", NULL, NULL); goto settle;
        }
        json_t *sc = sau2_schedule(c, ref);
        if (sau2_is_raise(sc)) {
            if (sau2_str_eq(json_obj_get(sc, "__raise__"), "AuthorityRefusal")) err = sc;
            else prop = sc;
            json_free(cnr); json_free(ref); json_free(rows); json_free(target); goto settle;
        }
        json_free(sc);
        json_set_hdr(results, sid ? sid : "", "ready");
        json_free(cnr); json_free(ref); json_free(rows); json_free(target);
        continue;
settle:
        if (prop) { json_free(results); return prop; }
        if (err) {
            json_set_hdr(results, sid ? sid : "", json_get_str(err, "reason", ""));
            json_free(err);
        }
    }
    return results;
}

/* PoP: sau2_admit_automation @ gateway/session_authority.py:admit_automation */
static json_t *sau2_admit_automation(sau2_ctx_t *c, const json_t *args) {
    json_t *a = json_object();
    json_t *adapter = json_obj_get(args, "adapter");
    json_set(a, "adapter", adapter ? json_copy(adapter) : json_null());
    json_t *event = json_obj_get(args, "event");
    json_set(a, "event", event ? json_copy(event) : json_null());
    json_t *identity = json_obj_get(args, "identity");
    json_set(a, "identity", identity ? json_copy(identity) : json_null());
    json_t *replaces = json_obj_get(args, "replaces");
    json_set(a, "replaces", replaces ? json_copy(replaces) : json_null());
    json_t *fold = json_obj_get(args, "fold");
    json_set(a, "fold", fold ? json_copy(fold) : json_null());
    return sau2_call(c, "admit_automation", a);
}

/* The _submit payload-merge helper: obj_update from a dict seam result. */
static void sau2_merge(json_t *dst, const json_t *src) {
    if (!src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
}

/* PoP: sau2_submit @ gateway/session_authority.py:submit */
static json_t *sau2_submit(sau2_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *request = json_obj_get(args, "request");
    json_t *ref = request ? json_obj_get(request, "ref") : NULL;
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *ar = sau2_authorize(c, actor, ref, "session:submit");
    SAU2_CHK(ar);
    json_free(ar);
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);  /* mutation_lock access */
    if (err) return err;
    (void)live;
    json_t *ar2 = sau2_authorize(c, actor, ref, "session:submit");
    SAU2_CHK(ar2);
    json_free(ar2);
    json_t *cl = sau2_call(c, "capture_lease", json_object());
    SAU2_CHK(cl);
    json_free(cl);
    /* queue_only = queue_while_restarting is True (identity) */
    json_t *qwr = json_obj_get(args, "queue_while_restarting");
    bool queue_only = qwr && qwr->type == JSON_BOOL && qwr->bool_val;
    json_t *ia = json_object();
    json_set(ia, "actor", sau2_actor_arg(actor));
    json_set(ia, "request", request ? json_copy(request) : json_null());
    json_set(ia, "queue_only", json_bool(queue_only));
    json_t *kc = json_obj_get(args, "_kanban_card");
    json_set(ia, "_kanban_card", kc ? json_copy(kc) : json_null());
    json_t *replaces = json_obj_get(args, "replaces");
    json_set(ia, "replaces", replaces ? json_copy(replaces) : json_null());
    json_t *supersedes = json_obj_get(args, "supersedes");
    json_set(ia, "supersedes", supersedes ? json_copy(supersedes) : json_null());
    json_t *api_target = json_obj_get(args, "api_target");
    json_set(ia, "api_target", api_target ? json_copy(api_target) : json_null());
    json_t *hosted = json_obj_get(args, "hosted");
    json_set(ia, "hosted", hosted ? json_copy(hosted) : json_null());
    json_t *ir = sau2_submit_inner(c, ia);
    json_free(ia);
    return ir;
}

/* PoP: sau2_submit_inner @ gateway/session_authority.py:_submit */
static json_t *sau2_submit_inner(sau2_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *request = json_obj_get(args, "request");
    json_t *ref = request ? json_obj_get(request, "ref") : NULL;
    const char *sid = json_get_str(ref, "session_id", "");
    const char *request_id = json_get_str(request, "request_id", "");
    const char *intent = json_get_str(request, "intent", "");
    json_t *req_payload = request ? json_obj_get(request, "payload") : NULL;
    bool queue_only = json_get_bool(args, "queue_only", false);
    json_t *ro = sau2_require_admission_open(c, queue_only);
    SAU2_CHK(ro);
    json_free(ro);
    /* validation: intent=='queue', {'text'} <= keys <= allowed, text is str */
    static const char *ALLOWED[] = {
        "text", "attachments", "finite", "unattended", "surface", "voice_context",
        "interrupted", "voice_turn", "display_kind", "title_preview", "turn_author",
        "api_text", "model_override", "clarify_batch"};
    bool invalid = strcmp(intent, "queue") != 0;
    if (!invalid) {
        if (!sau2_dict_has(req_payload, "text")) invalid = true;
        else if (req_payload && req_payload->type == JSON_OBJECT) {
            for (size_t i = 0; i < req_payload->c.count && !invalid; i++) {
                bool ok = false;
                for (size_t j = 0; j < sizeof(ALLOWED) / sizeof(ALLOWED[0]); j++)
                    if (strcmp(req_payload->c.keys[i], ALLOWED[j]) == 0) { ok = true; break; }
                if (!ok) invalid = true;
            }
            json_t *text = json_obj_get(req_payload, "text");
            if (!text || text->type != JSON_STRING) invalid = true;
        } else invalid = true;
    }
    if (invalid) return sau2_refusal("invalid_params", NULL, NULL);
    /* finite / surface / display / cli_turn / attachments seams */
    json_t *fa = json_object();
    json_set(fa, "payload", req_payload ? json_copy(req_payload) : json_null());
    json_t *finite = sau2_call(c, "admit_finite", fa);
    SAU2_CHK(finite);
    json_t *sa = json_object();
    json_set(sa, "payload", req_payload ? json_copy(req_payload) : json_null());
    json_t *surface = sau2_call(c, "admit_surface", sa);
    if (sau2_is_raise(surface)) { json_free(finite); return surface; }
    json_t *da = json_object();
    json_set(da, "payload", req_payload ? json_copy(req_payload) : json_null());
    json_t *display = sau2_call(c, "admit_display", da);
    if (sau2_is_raise(display)) { json_free(finite); json_free(surface); return display; }
    json_t *ca = json_object();
    json_set(ca, "payload", req_payload ? json_copy(req_payload) : json_null());
    json_t *cli_turn = sau2_call(c, "admit_cli_turn", ca);
    if (sau2_is_raise(cli_turn)) { json_free(finite); json_free(surface); json_free(display); return cli_turn; }
    json_t *aa = json_object();
    json_t *attachments = req_payload ? json_obj_get(req_payload, "attachments") : NULL;
    json_set(aa, "attachments", attachments ? json_copy(attachments) : json_null());
    json_set(aa, "admitted", json_bool(true));  /* the closure marker (opaque) */
    json_t *attach = sau2_call(c, "admit_attachments", aa);
    if (sau2_is_raise(attach)) {
        json_free(finite); json_free(surface); json_free(display); json_free(cli_turn); return attach;
    }
    /* payload = {'text': ..., **finite, **surface, **display, **cli_turn, **attach} */
    json_t *payload = json_object();
    json_t *text = json_obj_get(req_payload, "text");
    json_set(payload, "text", text ? json_copy(text) : json_null());
    sau2_merge(payload, finite); json_free(finite);
    sau2_merge(payload, surface); json_free(surface);
    sau2_merge(payload, display); json_free(display);
    sau2_merge(payload, cli_turn); json_free(cli_turn);
    sau2_merge(payload, attach); json_free(attach);
    json_t *kc = json_obj_get(args, "_kanban_card");
    if (kc && kc->type != JSON_NULL) json_set(payload, "kanban_card_v1", json_copy(kc));
    json_t *err = NULL;
    json_t *live = sau2_live_req(c, sid, &err);
    if (err) { json_free(payload); return err; }
    json_t *source = json_obj_get(live, "source");
    if (source && source->type != JSON_NULL) {
        json_t *soa = json_object();
        json_t *uid = json_obj_get(source, "user_id");
        json_set(soa, "a", uid ? json_copy(uid) : json_null());
        json_t *subj = actor ? json_obj_get(actor, "subject") : NULL;
        json_set(soa, "b", subj ? json_copy(subj) : json_null());
        json_t *so = sau2_call(c, "same_owner", soa);
        if (sau2_is_raise(so)) { json_free(payload); return so; }
        bool same = sau2_truthy(so);
        json_free(so);
        if (!same) {
            json_t *lo = json_object();
            json_t *pid = json_obj_get(c->state, "profile_id");
            json_set(lo, "profile_id", pid ? json_copy(pid) : json_null());
            json_set_hdr(lo, "session_id", sid);
            json_set(lo, "principal_id", subj ? json_copy(subj) : json_null());
            json_set(payload, "local_operator_v1", lo);
        }
    }
    /* admit closure: try admit + admission_row; except Exception -> release + raise */
    json_t *gate = sau2_gate_refusal(c, NULL, queue_only);
    json_t *admit_err = NULL;
    json_t *row = NULL;
    if (sau2_is_raise(gate)) { admit_err = gate; }
    else {
        json_t *adma = json_object();
        json_t *epoch = json_obj_get(c->state, "epoch");
        json_set(adma, "epoch", epoch ? json_copy(epoch) : json_null());
        json_t *asubj = actor ? json_obj_get(actor, "subject") : NULL;
        json_set(adma, "principal_id", asubj ? json_copy(asubj) : json_null());
        json_set_hdr(adma, "session_id", sid);
        json_set_hdr(adma, "request_id", request_id);
        json_set(adma, "payload", json_copy(payload));
        json_set_hdr(adma, "intent", intent);
        json_set(adma, "gate", gate);  /* owned */
        json_t *replaces = json_obj_get(args, "replaces");
        json_set(adma, "replaces", replaces ? json_copy(replaces) : json_null());
        json_t *supersedes = json_obj_get(args, "supersedes");
        json_set(adma, "supersedes", supersedes ? json_copy(supersedes) : json_null());
        json_t *api_target = json_obj_get(args, "api_target");
        json_set(adma, "api_target", api_target ? json_copy(api_target) : json_null());
        json_t *hosted = json_obj_get(args, "hosted");
        json_set(adma, "hosted", hosted ? json_copy(hosted) : json_null());
        json_t *admit = sau2_call(c, "store.admissions.admit", adma);
        if (sau2_is_raise(admit)) admit_err = admit;
        else {
            json_t *r0 = json_get(admit, 0);
            row = sau2_admission_row(c, r0);
            json_free(admit);
            if (sau2_is_raise(row)) { admit_err = row; row = NULL; }
        }
    }
    if (admit_err) {
        if (sau2_is_exception(admit_err)) {
            json_t *rc = sau2_release_refused_capture(c, request, payload);
            if (sau2_is_raise(rc)) { json_free(payload); json_free(admit_err); return rc; }
            json_free(rc);
            json_free(payload);
            return admit_err;
        }
        json_free(payload);
        return admit_err;  /* BaseException propagates without release */
    }
    /* tracked_write: then=_admitted(ref, None), after=_schedule_admitted(ref) */
    json_t *ad = sau2_admitted(c, ref, row);
    json_free(row);
    if (sau2_is_raise(ad)) { json_free(payload); return ad; }
    json_t *sc = sau2_schedule_admitted(c, ref);
    if (sau2_is_raise(sc)) { json_free(payload); json_free(ad); return sc; }
    json_free(sc);
    json_free(payload);
    return ad;
}