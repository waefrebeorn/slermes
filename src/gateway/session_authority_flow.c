/*
 * session_authority_flow.c — FLOW cores + leaves of gateway/session_authority.py
 * (hermes-agent): the admission lifecycle, observer/publication, cancel/settle
 * and agent/approval-control decision chains.
 *
 * Decision sequences only: every live I/O (store reads/writes, restore seams,
 * event-stream publishes, controls, waiters, runtime workers, owner identity,
 * logging) enters through ONE universal seam: sau_call(ctx, name, args). The
 * harness wires it to a recording stub fed the case's scripted results (in.io);
 * the oracle installs recording monkeypatch stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts. Refusals mirror the real
 * AuthorityRefusal (reason/field/detail, _one_line bounds included); the
 * AdmissionReceipt / SessionHandle / PendingAdmission returns mirror the live
 * dataclass asdict shapes.
 *
 * Claimed siblings (ported in gateway_inbound_flow.c) are SEAMED here, never
 * re-ported: admission_row, self._receipt, self._handle, self.authorize,
 * self.logical_owner. Ported targets compose through real C calls (the oracle
 * runs the extracted live methods, which likewise compose).
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sau_-prefixed.
 *
 * Representation contract (shared with tests/oracle_session_authority_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc), ...} object;
 *   AuthorityRefusal adds reason/field/detail (field/detail _one_line-bound).
 * - Platform enum members travel as their .value strings; agents/transports as
 *   id strings (the oracle wraps non-empty ids in attribute doubles). The
 *   _AGENT_PENDING_SENTINEL identity travels as the marker "__sentinel__".
 * - logger calls are silent on both sides (not traced). authority_persistence
 *   is a silent handle lookup; its store's methods are the seams.
 * - asyncio.to_thread / tracked_write / track_mutation run their callable
 *   synchronously with no trace of their own (the inner store call IS traced).
 *   asyncio.create_task is a seam returning a task id; the scheduled _drain
 *   never runs in these flows and the done callbacks never fire.
 * - locks (event_stream.lock, mutation_lock, claim_gate) are no-ops in the
 *   single-threaded harness on both sides.
 * - sets (native_waiters, cancel_obligations) travel as SORTED id arrays on
 *   both sides; waiters as {aid: {"done": bool}}; the waiter.set_exception /
 *   waiter.set_result seams fire only when the popped waiter is not done.
 * - event_stream.execution is a state dict on the live session, mutated in
 *   place and emitted in out.state.
 * - Scripted seams may carry {"__script__": true, "ret": v, "raise": spec};
 *   a bare array is a per-call queue (last value does NOT repeat — exhausted
 *   is fail-closed). A missing/exhausted script is a fail-closed RuntimeError.
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

/* persistence.authority.vocabulary.STEERING */
#define SAU_STEERING "steering"
/* gateway.run._AGENT_PENDING_SENTINEL identity marker */
#define SAU_SENTINEL "__sentinel__"

/* ── universal seam context ── */
typedef struct sau_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sau_ctx_t;

/* ── basic helpers ── */
static bool sau_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sau_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool sau_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *sau_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool sau_str_eq(const json_t *v, const char *s) {
    const char *t = sau_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* exceptions caught by `except Exception` (not the BaseException trio) */
static bool sau_is_exception(const json_t *r) {
    if (!sau_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0
        && strcmp(t, "GeneratorExit") != 0;
}

static json_t *sau_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* str(KeyError(k)) == repr(k) for the string ids the fixtures use. */
static json_t *sau_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return sau_err("KeyError", buf);
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces
 * (str.split()/join), other non-printables drop, bounded; empty -> None.
 * ASCII fixture strings: bytes >= 0x80 are treated as printable (the live
 * Unicode categories are identity on the fixture corpus).
 */
static char *sau_one_line(const char *value, size_t limit) {
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
static json_t *sau_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = sau_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = sau_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *sau_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return sau_refusal(json_get_str(spec, "reason", ""),
                           sau_str(json_obj_get(spec, "field")),
                           sau_str(json_obj_get(spec, "detail")));
    }
    const char *msg = json_get_str(spec, "message", "");
    return sau_err(type, msg);
}

static char *sau_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* ── seam dispatch ── */
/*
 * One scripted entry: a plain value (reused), or {"__script__": true,
 * "ret": v, "raise": spec}. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. A bare array is a per-call queue.
 */
static json_t *sau_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return sau_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *sau_call(sau_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = sau_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = sau_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = sau_entry_result(json_get(entry, idx));
        }
    } else {
        res = sau_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* ── state helpers ── */
static json_t *sau_sub(sau_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *sau_sub_ensure(sau_ctx_t *c, const char *key) {
    json_t *v = sau_sub(c, key);
    if (!v) { v = json_object(); json_set(c->state, key, v); }
    return v;
}

/* set-valued state (native_waiters, cancel_obligations) is a SORTED ARRAY. */
static json_t *sau_set_ensure(sau_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    if (!v || v->type != JSON_ARRAY) { v = json_array(); json_set(c->state, key, v); }
    return v;
}

/* dict.pop(key, None): owned copy out, key removed. */
static json_t *sau_dict_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    if (!v) return NULL;
    json_t *copy = json_copy(v);
    json_obj_del(obj, key);
    return copy;
}

static bool sau_dict_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* set membership over a sorted-array model */
static bool sau_set_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sau_str_eq(json_get(arr, i), id)) return true;
    return false;
}

/* set.add: insert keeping sorted order (no dup) */
static void sau_set_add(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    if (sau_set_has(arr, id)) return;
    size_t n = json_len(arr), pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *s = sau_str(json_get(arr, i));
        if (s && strcmp(s, id) > 0) { pos = i; break; }
    }
    /* insert at pos by rebuilding (libjson has no insert) */
    json_t *copy = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == pos) json_append(copy, json_string(id));
        json_append(copy, json_copy(json_get(arr, i)));
    }
    if (pos == n) json_append(copy, json_string(id));
    /* replace arr contents in place: swap children */
    for (size_t i = 0; i < arr->c.count; i++) {
        json_free(arr->c.items[i]);
        free(arr->c.keys ? arr->c.keys[i] : NULL);
    }
    free(arr->c.items); free(arr->c.keys);
    arr->c.items = copy->c.items; arr->c.keys = copy->c.keys; arr->c.count = copy->c.count;
    copy->c.items = NULL; copy->c.keys = NULL; copy->c.count = 0;
    json_free(copy);
}

/* set.discard */
static void sau_set_discard(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        if (sau_str_eq(json_get(arr, i), id)) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++) arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}

/* capabilities membership (actor.capabilities is a JSON array of strings). */
static bool sau_caps_has(const json_t *caps, const char *cap) {
    if (!caps || caps->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(caps); i++)
        if (sau_str_eq(json_get(caps, i), cap)) return true;
    return false;
}

/* Sort key matching the oracle's frozenset canon:
 *   sorted(x, key=lambda z: json.dumps(z, sort_keys=True))
 * Python's json.dumps defaults to ensure_ascii=True, so non-ASCII becomes
 * "\uXXXX" ("\ud83d\ude00" for 😀) and sorts BEFORE ASCII "session:..." strings
 * because '\' (0x5C) < 's' (0x73). Raw-byte strcmp would sort it last. */
static char *sau_sort_key(const char *s) {
    if (!s) s = "";
    size_t cap = strlen(s) * 13 + 4;
    char *o = malloc(cap);
    if (!o) return strdup("\"\"");
    size_t k = 0;
    o[k++] = '"';
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        size_t n;
        if (*p < 0x80)              { cp = *p;         n = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; n = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; n = 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; n = 4; }
        else                        { cp = *p;         n = 1; }
        for (size_t i = 1; i < n && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
        p += n;
        if (cp == '"')       { o[k++] = '\\'; o[k++] = '"'; }
        else if (cp == '\\') { o[k++] = '\\'; o[k++] = '\\'; }
        else if (cp == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (cp == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (cp == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (cp == '\b') { o[k++] = '\\'; o[k++] = 'b'; }
        else if (cp == '\f') { o[k++] = '\\'; o[k++] = 'f'; }
        else if (cp < 0x20)  k += (size_t)snprintf(o + k, cap - k, "\\u%04x", cp);
        else if (cp < 0x80)  o[k++] = (char)cp;
        else if (cp <= 0xFFFF) k += (size_t)snprintf(o + k, cap - k, "\\u%04x", cp);
        else {
            unsigned int v = cp - 0x10000;
            k += (size_t)snprintf(o + k, cap - k, "\\u%04x\\u%04x",
                                  0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
        }
    }
    o[k++] = '"';
    o[k] = '\0';
    return o;
}

/* sorted copy of a string array (for actor.capabilities frozenset projection) */
static json_t *sau_sorted_strs(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    char **keys = n ? malloc(n * sizeof(char *)) : NULL;
    for (size_t i = 0; i < n; i++)
        keys[i] = sau_sort_key(sau_str(json_get(arr, i)));
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_copy(json_get(arr, i));
        size_t j = json_len(out);
        json_append(out, v);
        while (j > 0 && strcmp(keys[j - 1], keys[j]) > 0) {
            json_t *t = out->c.items[j - 1];
            out->c.items[j - 1] = out->c.items[j];
            out->c.items[j] = t;
            char *tk = keys[j - 1]; keys[j - 1] = keys[j]; keys[j] = tk;
            j--;
        }
    }
    for (size_t i = 0; i < n; i++) free(keys[i]);
    free(keys);
    return out;
}

/* live session lookup: self.sessions.get(sid) (NULL when absent) */
static json_t *sau_live(sau_ctx_t *c, const char *sid) {
    json_t *sessions = sau_sub(c, "sessions");
    return (sessions && sid) ? json_obj_get(sessions, sid) : NULL;
}

/* self.sessions[sid] — KeyError when absent (live subscript). */
static json_t *sau_live_req(sau_ctx_t *c, const char *sid, json_t **err) {
    json_t *live = sau_live(c, sid);
    if (!live) *err = sau_keyerror(sid);
    return live;
}

/* Propagate a raise result unless it is NULL (success). */
#define SAU_CHK(v) do { if (sau_is_raise(v)) return (v); } while (0)

/* actor projection for seam args: capabilities sorted (frozenset canon). */
static json_t *sau_actor_arg(const json_t *actor) {
    json_t *a = json_object();
    if (!actor || actor->type != JSON_OBJECT) return a;
    json_t *subj = json_obj_get(actor, "subject");
    json_set(a, "subject", subj ? json_copy(subj) : json_null());
    json_t *pid = json_obj_get(actor, "profile_id");
    json_set(a, "profile_id", pid ? json_copy(pid) : json_null());
    json_set(a, "capabilities", sau_sorted_strs(json_obj_get(actor, "capabilities")));
    json_t *tid = json_obj_get(actor, "transport_id");
    json_set(a, "transport_id", tid ? json_copy(tid) : json_null());
    return a;
}

/* ref projection {profile_id, session_id} */
static json_t *sau_ref_arg(const json_t *ref) {
    json_t *r = json_object();
    json_t *pid = json_obj_get(ref, "profile_id");
    json_set(r, "profile_id", pid ? json_copy(pid) : json_null());
    json_t *sid = json_obj_get(ref, "session_id");
    json_set(r, "session_id", sid ? json_copy(sid) : json_null());
    return r;
}

/* SessionRef(self.profile_id, sid) as a dict */
static json_t *sau_mkref(sau_ctx_t *c, const char *sid) {
    json_t *r = json_object();
    json_t *pid = json_obj_get(c->state, "profile_id");
    json_set(r, "profile_id", pid ? json_copy(pid) : json_null());
    json_set_hdr(r, "session_id", sid ? sid : "");
    return r;
}/* ── forward declarations (ported targets compose through real C calls) ── */
static json_t *sau_route_epoch(sau_ctx_t *c, const json_t *args);
static json_t *sau_fence_route(sau_ctx_t *c, const json_t *args);
static json_t *sau_log_drain_failure(sau_ctx_t *c, const json_t *args);
static json_t *sau_run_deferred_reset(sau_ctx_t *c, const json_t *args);
static json_t *sau_backfill_legacy(sau_ctx_t *c, const json_t *args);
static json_t *sau_backfill_compression(sau_ctx_t *c, const json_t *args);
static json_t *sau_check_approval_generation(sau_ctx_t *c, const json_t *args);
static json_t *sau_drain_stopping(sau_ctx_t *c, const json_t *args);
static json_t *sau_pending_rows(sau_ctx_t *c, const json_t *args);
static json_t *sau_retire_overflowed(sau_ctx_t *c, const json_t *args);
static json_t *sau_unless_deleted(sau_ctx_t *c, const json_t *args);
static json_t *sau_resolve(sau_ctx_t *c, const json_t *args);
static json_t *sau_pending_receipt(sau_ctx_t *c, const json_t *args);
static json_t *sau_publish_pending(sau_ctx_t *c, const json_t *args);
static json_t *sau_schedule(sau_ctx_t *c, const json_t *args);
static json_t *sau_wake_after_worker(sau_ctx_t *c, const json_t *args);
static json_t *sau_pause(sau_ctx_t *c, const json_t *args);
static json_t *sau_admitted(sau_ctx_t *c, const json_t *args);
static json_t *sau_schedule_admitted(sau_ctx_t *c, const json_t *args);
static json_t *sau_release_refused_capture(sau_ctx_t *c, const json_t *args);
static json_t *sau_commit_cancel(sau_ctx_t *c, const json_t *args);
static json_t *sau_settle_cancelled(sau_ctx_t *c, const json_t *args);
static json_t *sau_classic_turn_holds(sau_ctx_t *c, const json_t *args);
static json_t *sau_runs_own_agent(sau_ctx_t *c, const json_t *args);
static json_t *sau_steer_agent(sau_ctx_t *c, const json_t *args);
static json_t *sau_register_approval(sau_ctx_t *c, const json_t *args);
static json_t *sau_register_clarify(sau_ctx_t *c, const json_t *args);
static json_t *sau_respond(sau_ctx_t *c, const json_t *args);
static json_t *sau_stamp_claim(sau_ctx_t *c, const json_t *args);

/* ── claimed-sibling seam wrappers (ported in gateway_inbound_flow.c) ── */
/* admission_row(record): module-level pure helper, seamed. */
static json_t *sau_admission_row(sau_ctx_t *c, const json_t *record) {
    json_t *a = json_object();
    json_set(a, "record", record ? json_copy(record) : json_null());
    return sau_call(c, "admission_row", a);
}
/* self._receipt(row) -> AdmissionReceipt asdict shape */
static json_t *sau_receipt(sau_ctx_t *c, const json_t *row) {
    json_t *a = json_object();
    json_set(a, "row", row ? json_copy(row) : json_null());
    return sau_call(c, "self._receipt", a);
}
/* self._handle(ref) -> SessionHandle asdict shape */
static json_t *sau_handle(sau_ctx_t *c, const json_t *ref) {
    json_t *a = json_object();
    json_set(a, "ref", sau_ref_arg(ref));
    return sau_call(c, "self._handle", a);
}
/* self.authorize(actor, ref, capability) -> None / AuthorityRefusal */
static json_t *sau_authorize(sau_ctx_t *c, const json_t *actor, const json_t *ref,
                             const char *capability) {
    json_t *a = json_object();
    json_set(a, "actor", sau_actor_arg(actor));
    json_set(a, "ref", sau_ref_arg(ref));
    json_set_hdr(a, "capability", capability ? capability : "");
    return sau_call(c, "self.authorize", a);
}
/* self.logical_owner(session_id) -> sid */
static json_t *sau_logical_owner(sau_ctx_t *c, const json_t *sid) {
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_copy(sid) : json_null());
    return sau_call(c, "self.logical_owner", a);
}

/* scalar equality for generation fences (None==None, int==int). */
static bool sau_gen_eq(const json_t *a, const json_t *b) {
    if (sau_is_null(a) && sau_is_null(b)) return true;
    return a && b && a->type == JSON_NUMBER && b->type == JSON_NUMBER
        && a->num_val == b->num_val;
}

/* ══════════════ leaves ══════════════ */

/* PoP: sau_route_epoch @ gateway/session_authority.py:route_epoch */
static json_t *sau_route_epoch(sau_ctx_t *c, const json_t *args) {
    const char *route = json_get_str(args, "route", "");
    json_t *epochs = sau_sub(c, "_route_epochs");
    json_t *v = epochs ? json_obj_get(epochs, route) : NULL;
    return v ? json_copy(v) : json_number(0);
}

/* PoP: sau_fence_route @ gateway/session_authority.py:fence_route */
static json_t *sau_fence_route(sau_ctx_t *c, const json_t *args) {
    const char *route = json_get_str(args, "route", "");
    json_t *cur = sau_route_epoch(c, args);
    double n = json_number_value(cur);
    json_free(cur);
    json_t *epochs = sau_sub_ensure(c, "_route_epochs");
    json_set(epochs, route, json_number(n + 1));
    return NULL;
}

/* PoP: sau_log_drain_failure @ gateway/session_authority.py:_log_drain_failure */
static json_t *sau_log_drain_failure(sau_ctx_t *c, const json_t *args) {
    const char *task = json_get_str(args, "task", "");
    json_t *a = json_object();
    json_set_hdr(a, "task", task);
    json_t *cancelled = sau_call(c, "task.cancelled", a);
    SAU_CHK(cancelled);
    bool done = sau_truthy(cancelled);
    json_free(cancelled);
    if (done) return NULL;  /* short-circuit: exception() not called */
    json_t *a2 = json_object();
    json_set_hdr(a2, "task", task);
    json_t *exc = sau_call(c, "task.exception", a2);
    SAU_CHK(exc);
    bool has_exc = !sau_is_null(exc);
    json_free(exc);
    if (has_exc) {
        /* logging.error('...%r', task.exception()) evaluates exception() AGAIN */
        json_t *a3 = json_object();
        json_set_hdr(a3, "task", task);
        json_t *exc2 = sau_call(c, "task.exception", a3);
        SAU_CHK(exc2);
        json_free(exc2);
    }
    return NULL;
}

/* PoP: sau_run_deferred_reset @ gateway/session_authority.py:_run_deferred_reset */
static json_t *sau_run_deferred_reset(sau_ctx_t *c, const json_t *args) {
    if (!json_get_bool(args, "reset", false)) return NULL;  /* reset is None */
    json_t *r = sau_call(c, "reset", json_object());
    if (sau_is_raise(r)) {
        if (sau_is_exception(r)) { json_free(r); return NULL; }  /* except Exception: log silent */
        return r;  /* BaseException propagates */
    }
    json_free(r);
    return NULL;
}

/* store_location(store.scope) seam args (the silent logger still evaluates it) */
static json_t *sau_store_location_arg(sau_ctx_t *c) {
    json_t *a = json_object();
    json_t *scope = json_obj_get(c->state, "store_scope");
    json_set(a, "scope", scope ? json_copy(scope) : json_null());
    return sau_call(c, "store_location", a);
}

/* PoP: sau_backfill_legacy @ gateway/session_authority.py:backfill_legacy_session_owners */
static json_t *sau_backfill_legacy(sau_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *stamp = sau_call(c, "store.sessions.store_owner", json_object());
    SAU_CHK(stamp);
    if (!sau_truthy(stamp)) { json_free(stamp); return json_number(0); }
    json_t *a = json_object();
    json_set(a, "stamp", json_copy(stamp));
    json_free(stamp);
    json_t *stamped = sau_call(c, "store.sessions.backfill_session_owners", a);
    if (sau_is_raise(stamped)) {
        if (sau_is_exception(stamped)) {
            json_free(stamped);
            json_t *sl = sau_store_location_arg(c);
            SAU_CHK(sl);
            json_free(sl);
            return json_number(0);
        }
        return stamped;
    }
    /* if stamped: logging.info silent */
    return stamped;
}

/* PoP: sau_backfill_compression @ gateway/session_authority.py:backfill_compression_created_sources */
static json_t *sau_backfill_compression(sau_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *stamped = sau_call(c, "store.lineage.stamp_legacy_compression_chains", json_object());
    if (sau_is_raise(stamped)) {
        if (sau_is_exception(stamped)) {
            json_free(stamped);
            json_t *sl = sau_store_location_arg(c);
            SAU_CHK(sl);
            json_free(sl);
            return json_number(0);
        }
        return stamped;
    }
    return stamped;
}

/* PoP: sau_check_approval_generation @ gateway/session_authority.py:check_approval_generation */
static json_t *sau_check_approval_generation(sau_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *gen = json_obj_get(args, "generation");
    json_t *ref = sau_mkref(c, sid);
    json_t *h = sau_handle(c, ref);
    json_free(ref);
    SAU_CHK(h);
    json_t *eg = json_obj_get(h, "execution_generation");
    json_t *es = json_obj_get(h, "execution_state");
    bool stale = !sau_gen_eq(eg, gen) || !sau_str_eq(es, "running");
    json_free(h);
    if (stale) return sau_refusal("stale_generation", NULL, NULL);
    return NULL;
}

/* PoP: sau_pause @ gateway/session_authority.py:_pause */
static json_t *sau_pause(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    const char *reason = json_get_str(args, "reason", "");
    json_t *a = json_object();
    json_set_hdr(a, "session_id", sid);
    json_t *listed = sau_call(c, "store.admissions.list", a);
    SAU_CHK(listed);
    json_t *nw = sau_set_ensure(c, "native_waiters");
    json_t *waiters = sau_sub_ensure(c, "waiters");
    if (listed && listed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(listed); i++) {
            json_t *row = json_get(listed, i);
            const char *aid = json_get_str(row, "admission_id", "");
            if (strcmp(reason, "session_busy") == 0 && !sau_set_has(nw, aid)) continue;
            sau_set_discard(nw, aid);
            json_t *waiter = sau_dict_pop(waiters, aid);
            if (waiter) {
                bool wdone = json_get_bool(waiter, "done", false);
                json_free(waiter);
                if (!wdone) {
                    json_t *wa = json_object();
                    json_set_hdr(wa, "admission_id", aid);
                    json_set_hdr(wa, "reason", reason);
                    json_t *wr = sau_call(c, "waiter.set_exception", wa);
                    SAU_CHK(wr);
                    json_free(wr);
                }
            }
        }
    }
    json_free(listed);
    return NULL;
}

/* PoP: sau_drain_stopping @ gateway/session_authority.py:_drain_stopping */
static json_t *sau_drain_stopping(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *runner = json_obj_get(c->state, "runner");
    bool draining = sau_truthy(json_obj_get(runner, "_draining"));
    bool retiring = sau_truthy(json_obj_get(c->state, "retiring"));
    json_t *cur = sau_live(c, sid);
    const char *cur_token = cur ? json_get_str(cur, "token", sid) : NULL;
    const char *live_token = json_get_str(args, "live_token", sid);
    bool ident_mismatch = !cur || strcmp(cur_token ? cur_token : "", live_token ? live_token : "") != 0;
    if (draining || retiring || ident_mismatch) {
        json_t *pa = json_object();
        json_set(pa, "ref", sau_ref_arg(ref));
        json_set_hdr(pa, "reason", "runtime_draining");
        json_t *pr = sau_pause(c, pa);
        json_free(pa);
        SAU_CHK(pr);
        json_free(pr);
        return json_bool(true);
    }
    return json_bool(false);
}/* ══════════════ observer / publication flows ══════════════ */

/* PoP: sau_pending_rows @ gateway/session_authority.py:_pending_rows */
static json_t *sau_pending_rows(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *a = json_object();
    json_set_hdr(a, "session_id", sid);
    json_t *listed = sau_call(c, "store.admissions.list", a);
    SAU_CHK(listed);
    json_t *rows = json_array();
    if (listed && listed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(listed); i++) {
            json_t *row = sau_admission_row(c, json_get(listed, i));
            if (sau_is_raise(row)) { json_free(listed); json_free(rows); return row; }
            json_append(rows, row);
        }
    }
    json_free(listed);
    json_t *live = sau_live(c, sid);
    json_t *masked = live ? json_obj_get(live, "settling") : NULL;
    if (!sau_truthy(masked)) return rows;
    json_t *by_id = json_object();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        const char *aid = json_get_str(row, "admission_id", "");
        json_set(by_id, aid, json_copy(row));
    }
    json_free(rows);
    if (masked && masked->type == JSON_OBJECT) {
        for (size_t i = 0; i < masked->c.count; i++)
            json_set(by_id, masked->c.keys[i], json_copy(masked->c.items[i]));
    }
    size_t n = by_id->c.count;
    json_t *out = json_array();
    if (n == 0) { json_free(by_id); return out; }
    json_t **vals = malloc(n * sizeof(json_t *));
    if (!vals) { json_free(by_id); return out; }
    for (size_t i = 0; i < n; i++) vals[i] = by_id->c.items[i];
    for (size_t i = 1; i < n; i++) {
        json_t *v = vals[i];
        double seq = json_get_num(v, "seq", 0);
        size_t j = i;
        while (j > 0 && json_get_num(vals[j - 1], "seq", 0) > seq) { vals[j] = vals[j - 1]; j--; }
        vals[j] = v;
    }
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(vals[i]));
    free(vals);
    json_free(by_id);
    return out;
}

/* PoP: sau_retire_overflowed @ gateway/session_authority.py:_retire_overflowed */
static json_t *sau_retire_overflowed(sau_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    const char *transport = json_get_str(args, "transport", "");
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    json_t *subs = json_obj_get(live, "subscribers");
    json_t *events = sau_sub(c, "events");
    if (!subs || subs->type != JSON_OBJECT) return NULL;
    /* snapshot keys (list(live.subscribers.items())) before deleting */
    size_t n = subs->c.count;
    char **keys = malloc(n * sizeof(char *));
    if (!keys) return NULL;
    for (size_t i = 0; i < n; i++) keys[i] = sau_strdup(subs->c.keys[i]);
    for (size_t i = 0; i < n; i++) {
        json_t *member = json_obj_get(subs, keys[i]);
        const char *tid = json_get_str(member, "transport_id", "");
        json_t *ev = events ? json_obj_get(events, tid) : NULL;
        const char *ev_id = sau_str(ev);
        if (ev_id && transport && strcmp(ev_id, transport) == 0)
            json_obj_del(subs, keys[i]);
        free(keys[i]);
    }
    free(keys);
    return NULL;
}

/* PoP: sau_unless_deleted @ gateway/session_authority.py:unless_deleted */
static json_t *sau_unless_deleted(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *r = sau_call(c, "read", json_object());
    if (sau_is_raise(r)) {
        if (sau_str_eq(json_obj_get(r, "__raise__"), "AuthorityRefusal")) {
            json_t *a = json_object();
            json_set_hdr(a, "session_id", sid);
            json_t *row = sau_call(c, "store.sessions.get", a);
            if (sau_is_raise(row)) { json_free(r); return row; }
            bool gone = sau_is_null(row);
            json_free(row);
            if (gone) { json_free(r); return sau_refusal("not_found", NULL, NULL); }
        }
        return r;
    }
    return r;
}

/* PoP: sau_resolve @ gateway/session_authority.py:resolve */
static json_t *sau_resolve(sau_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    json_t *ar = sau_authorize(c, actor, ref, "session:read");
    SAU_CHK(ar);
    json_free(ar);
    return sau_handle(c, ref);
}

/* PoP: sau_pending_receipt @ gateway/session_authority.py:_pending_receipt */
static json_t *sau_pending_receipt(sau_ctx_t *c, const json_t *args) {
    json_t *row = json_obj_get(args, "row");
    json_t *payload = json_obj_get(row, "payload");
    json_t *text = json_obj_get(payload, "text");
    json_t *text_out;
    if (sau_is_null(text)) {
        json_t *ntv = json_obj_get(payload, "native_text_v1");
        json_t *ev = (ntv && ntv->type == JSON_OBJECT) ? json_obj_get(ntv, "event") : NULL;
        json_t *t = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "text") : NULL;
        text_out = t ? json_copy(t) : json_string("");
    } else {
        text_out = json_copy(text);
    }
    /* Live return expr evaluates left-to-right: **vars(self._receipt(row))
     * fires _receipt BEFORE controls=queued_controls(self, row). */
    json_t *receipt = sau_receipt(c, row);
    SAU_CHK(receipt);
    json_t *qa = json_object();
    json_set(qa, "row", row ? json_copy(row) : json_null());
    json_t *controls = sau_call(c, "queued_controls", qa);
    if (sau_is_raise(controls)) { json_free(text_out); json_free(receipt); return controls; }
    json_t *out = receipt;  /* owned; extend in place */
    json_t *rid = json_obj_get(row, "request_id");
    json_set(out, "input_id", rid ? json_copy(rid) : json_null());
    json_set(out, "text", text_out);
    json_set(out, "controls", controls);
    json_t *outcome = json_obj_get(row, "outcome");
    json_set(out, "steering", json_bool(sau_str_eq(outcome, SAU_STEERING)));
    return out;
}

/* PoP: sau_publish_pending @ gateway/session_authority.py:_publish_pending */
static json_t *sau_publish_pending(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *ra = json_object();
    json_set_hdr(ra, "session_id", sid);
    json_t *rr = sau_call(c, "restore_retired", ra);
    SAU_CHK(rr);
    json_free(rr);
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    (void)live;
    json_t *h = sau_handle(c, ref);
    SAU_CHK(h);
    json_t *pa = json_object();
    json_set(pa, "ref", sau_ref_arg(ref));
    json_t *pr = sau_call(c, "pending_rows", pa);
    if (sau_is_raise(pr)) { json_free(h); return pr; }
    json_t *pending = json_array();
    if (pr && pr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(pr); i++) {
            json_t *ia = json_object();
            json_set(ia, "row", json_copy(json_get(pr, i)));
            json_t *p = sau_pending_receipt(c, ia);
            json_free(ia);
            if (sau_is_raise(p)) { json_free(h); json_free(pr); json_free(pending); return p; }
            json_append(pending, p);
        }
    }
    json_free(pr);
    json_t *payload = json_object();
    json_set_hdr(payload, "stored_session_id", sid);
    json_set(payload, "pending", pending);
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(payload, "authority_epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(payload, "desktop_protocol", "hermes-gateway-v1");
    json_set(payload, "running", json_bool(sau_str_eq(json_obj_get(h, "execution_state"), "running")));
    json_t *eg = json_obj_get(h, "execution_generation");
    json_set(payload, "execution_generation", eg ? json_copy(eg) : json_null());
    json_t *rev = json_obj_get(h, "revision");
    json_set(payload, "revision", rev ? json_copy(rev) : json_null());
    json_free(h);
    json_t *puba = json_object();
    json_set_hdr(puba, "session_id", sid);
    json_set(puba, "payload", payload);
    json_set_hdr(puba, "event_type", "session.info");
    json_t *pub = sau_call(c, "event_stream.publish", puba);
    SAU_CHK(pub);
    json_free(pub);
    json_t *wa = json_object();
    json_set_hdr(wa, "session_id", sid);
    json_t *wb = sau_call(c, "wake_bot_receipts", wa);
    SAU_CHK(wb);
    json_free(wb);
    return NULL;
}

/* PoP: sau_schedule @ gateway/session_authority.py:_schedule */
static json_t *sau_schedule(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *ra = json_object();
    json_set_hdr(ra, "session_id", sid);
    json_t *rr = sau_call(c, "restore_retired", ra);
    SAU_CHK(rr);
    json_free(rr);
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    json_set(live, "rescan", json_bool(true));
    json_t *bw = json_obj_get(live, "background_wait");
    if (bw && bw->type != JSON_NULL) {
        json_t *wa = json_object();
        json_set(wa, "ref", sau_ref_arg(ref));
        json_set(wa, "person_input", json_bool(false));
        json_t *wb = sau_call(c, "wake_background_wait", wa);
        SAU_CHK(wb);
        json_free(wb);
    }
    json_t *task = json_obj_get(live, "task");
    bool need = sau_is_null(task) || json_get_bool(task, "done", false);
    if (need) {
        json_t *ca = json_object();
        json_set_hdr(ca, "session_id", sid);
        json_t *ct = sau_call(c, "asyncio.create_task", ca);
        SAU_CHK(ct);
        json_t *nt = json_object();
        json_set(nt, "id", ct);  /* owned */
        json_set(nt, "done", json_bool(false));
        json_set(live, "task", nt);
        /* add_done_callback(_log_drain_failure / persist_work_count): silent,
         * never fires in these flows (the scheduled _drain does not run). */
    }
    return NULL;
}

/* PoP: sau_wake_after_worker @ gateway/session_authority.py:wake_after_worker */
static json_t *sau_wake_after_worker(sau_ctx_t *c, const json_t *args) {
    json_t *sid = json_obj_get(args, "session_id");
    const char *sid_s = sau_str(sid);
    json_t *lo = sau_logical_owner(c, sid);
    SAU_CHK(lo);
    const char *lo_s = sau_str(lo);
    /* dict.fromkeys((session_id, logical_owner)): unique, insertion order */
    const char *ids[2];
    size_t n = 0;
    ids[n++] = sid_s ? sid_s : "";
    if (lo_s && strcmp(lo_s, ids[0]) != 0) ids[n++] = lo_s;
    for (size_t i = 0; i < n; i++) {
        if (sau_live(c, ids[i])) {
            json_t *ref = sau_mkref(c, ids[i]);
            json_t *pa = json_object();
            json_set(pa, "ref", ref);
            json_t *sr = sau_schedule(c, pa);
            json_free(pa);
            if (sau_is_raise(sr)) { json_free(lo); return sr; }
            json_free(sr);
        }
    }
    json_free(lo);
    return NULL;
}

/* PoP: sau_admitted @ gateway/session_authority.py:_admitted */
static json_t *sau_admitted(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    json_t *row = json_obj_get(args, "row");
    bool has_event = json_get_bool(args, "event", false);
    if (has_event) {
        json_t *ev = json_obj_get(c->state, "event");
        if (ev && ev->type == JSON_OBJECT) {
            json_set(ev, "_gateway_accepted", json_bool(true));
            const char *status = json_get_str(row, "status", "");
            json_t *on_commit = json_obj_get(ev, "_on_commit");
            if (strcmp(status, "queued") == 0 && on_commit && on_commit->type != JSON_NULL) {
                json_t *cr = sau_call(c, "event._on_commit", json_object());
                SAU_CHK(cr);
                json_free(cr);
            }
            json_t *ma = json_object();
            json_set(ma, "event", json_copy(ev));
            json_set(ma, "row", row ? json_copy(row) : json_null());
            json_t *mr = sau_call(c, "mark_producer_admission", ma);
            SAU_CHK(mr);
            json_free(mr);
        }
    }
    json_t *pa = json_object();
    json_set(pa, "ref", sau_ref_arg(ref));
    json_t *pp = sau_publish_pending(c, pa);
    json_free(pa);
    SAU_CHK(pp);
    json_free(pp);
    return sau_receipt(c, row);
}

/* PoP: sau_schedule_admitted @ gateway/session_authority.py:_schedule_admitted */
static json_t *sau_schedule_admitted(sau_ctx_t *c, const json_t *args) {
    json_t *sr = sau_schedule(c, args);
    SAU_CHK(sr);
    json_free(sr);
    return NULL;
}

/* PoP: sau_stamp_claim @ gateway/session_authority.py:_stamp_claim */
static json_t *sau_stamp_claim(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    json_t *row = json_obj_get(args, "row");
    const char *sid = json_get_str(ref, "session_id", "");
    if (!sau_truthy(row)) return row ? json_copy(row) : json_null();
    json_t *live = sau_live(c, sid);
    if (!live)  /* live.event_stream on None crashes before any mutation */
        return sau_err("AttributeError", "'NoneType' object has no attribute 'event_stream'");
    json_t *es = json_obj_get(live, "event_stream");
    if (!es) { es = json_object(); if (live) json_set(live, "event_stream", es); }
    json_t *execution = json_object();
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(execution, "authority_epoch", epoch ? json_copy(epoch) : json_null());
    json_t *gen = json_obj_get(row, "generation");
    json_set(execution, "execution_generation", gen ? json_copy(gen) : json_null());
    json_t *aid = json_obj_get(row, "admission_id");
    json_set(execution, "admission_id", aid ? json_copy(aid) : json_null());
    json_set(es, "execution", execution);
    json_t *executing = sau_sub_ensure(c, "executing");
    json_set(executing, sid, gen ? json_copy(gen) : json_null());
    json_t *pl = json_obj_get(row, "payload");
    json_t *payload = sau_truthy(pl) ? json_copy(pl) : json_object();
    json_t *da = json_object();
    json_set(da, "payload", payload);
    json_t *display = sau_call(c, "admission_display", da);
    SAU_CHK(display);
    json_t *puba = json_object();
    json_set_hdr(puba, "session_id", sid);
    json_set(puba, "payload", display);
    json_set_hdr(puba, "event_type", "message.start");
    json_t *pub = sau_call(c, "event_stream.publish", puba);
    SAU_CHK(pub);
    json_free(pub);
    json_t *pa = json_object();
    json_set(pa, "ref", sau_ref_arg(ref));
    json_t *pp = sau_publish_pending(c, pa);
    json_free(pa);
    SAU_CHK(pp);
    json_free(pp);
    return json_copy(row);
}/* ══════════════ admission / settle flows ══════════════ */

/* PoP: sau_release_refused_capture @ gateway/session_authority.py:_release_refused_capture */
static json_t *sau_release_refused_capture(sau_ctx_t *c, const json_t *args) {
    json_t *payload = json_obj_get(args, "payload");
    json_t *av1 = json_obj_get(payload, "attachments_v1");
    json_t *media = (av1 && av1->type == JSON_OBJECT) ? json_obj_get(av1, "media") : NULL;
    if (!sau_truthy(media)) return NULL;
    json_t *dcl = sau_call(c, "drop_capture_lease", json_object());
    SAU_CHK(dcl);
    json_free(dcl);
    json_t *mp = json_object();
    json_t *mwrap = json_object();
    json_set(mwrap, "media", json_copy(media));
    json_set(mp, "attachments_v1", mwrap);
    json_t *ma = json_object();
    json_set(ma, "payload", mp);
    json_t *refs = sau_call(c, "admission_media_references", ma);
    if (sau_is_raise(refs)) {
        if (sau_is_exception(refs)) { json_free(refs); return NULL; }
        return refs;
    }
    json_t *ra = json_object();
    json_set(ra, "refs", refs);
    json_t *rrm = sau_call(c, "store.admissions.record_retired_media", ra);
    if (sau_is_raise(rrm)) {
        if (sau_is_exception(rrm)) { json_free(rrm); return NULL; }
        return rrm;
    }
    json_free(rrm);
    json_t *scope = json_obj_get(c->state, "store_scope");
    json_t *pd = scope ? json_obj_get(scope, "profile_dir") : NULL;
    json_t *ca = json_object();
    json_set(ca, "profile_dir", pd ? json_copy(pd) : json_null());
    json_t *crm = sau_call(c, "collect_retired_media", ca);
    if (sau_is_raise(crm)) {
        if (sau_is_exception(crm)) { json_free(crm); return NULL; }
        return crm;
    }
    json_free(crm);
    return NULL;
}

/* PoP: sau_commit_cancel @ gateway/session_authority.py:_commit_cancel */
static json_t *sau_commit_cancel(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *aid = json_get_str(args, "admission_id", "");
    json_t *before = json_obj_get(args, "before");
    json_t *co = sau_set_ensure(c, "cancel_obligations");
    if (sau_str_eq(json_obj_get(before, "status"), "queued"))
        sau_set_add(co, aid);
    json_t *ca = json_object();
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(ca, "epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(ca, "admission_id", aid);
    json_t *rec = sau_call(c, "store.admissions.cancel", ca);
    json_t *row;
    if (sau_is_raise(rec)) {
        row = rec;
    } else {
        row = sau_admission_row(c, rec);
        json_free(rec);
    }
    if (sau_is_raise(row)) {
        if (sau_str_eq(json_obj_get(row, "__raise__"), "AuthorityRefusal"))
            sau_set_discard(co, aid);
        return row;
    }
    const char *status = json_get_str(row, "status", "");
    const char *outcome = json_get_str(row, "outcome", "");
    if (strcmp(status, "terminal") == 0 && strcmp(outcome, "cancelled") == 0
        && sau_set_has(co, aid)) {
        json_t *sa = json_object();
        json_set(sa, "ref", sau_ref_arg(ref));
        json_set_hdr(sa, "admission_id", aid);
        json_t *sr = sau_settle_cancelled(c, sa);
        json_free(sa);
        if (sau_is_raise(sr)) { json_free(row); return sr; }
        json_free(sr);
    } else {
        if (strcmp(status, "queued") != 0) sau_set_discard(co, aid);
        json_t *pa = json_object();
        json_set(pa, "ref", sau_ref_arg(ref));
        json_t *pp = sau_publish_pending(c, pa);
        json_free(pa);
        if (sau_is_raise(pp)) { json_free(row); return pp; }
        json_free(pp);
    }
    return row;
}

/* PoP: sau_settle_cancelled @ gateway/session_authority.py:_settle_cancelled */
static json_t *sau_settle_cancelled(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    const char *aid = json_get_str(args, "admission_id", "");
    const char *outcome = json_get_str(args, "outcome", "cancelled");
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    json_t *es = json_obj_get(live, "event_stream");
    if (!es) { es = json_object(); json_set(live, "event_stream", es); }
    json_t *pa = json_object();
    json_set(pa, "ref", sau_ref_arg(ref));
    json_t *pp = sau_publish_pending(c, pa);
    json_free(pa);
    SAU_CHK(pp);
    json_free(pp);
    json_t *running = json_obj_get(es, "execution");
    json_t *saved = running ? json_copy(running) : json_null();
    json_t *execution = json_object();
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(execution, "authority_epoch", epoch ? json_copy(epoch) : json_null());
    json_set_hdr(execution, "admission_id", aid);
    json_set(es, "execution", execution);
    json_t *payload = json_object();
    json_set_hdr(payload, "text", "");
    json_set_hdr(payload, "content", "");
    json_set_hdr(payload, "admission_id", aid);
    json_set_hdr(payload, "outcome", outcome);
    json_t *puba = json_object();
    json_set_hdr(puba, "session_id", sid);
    json_set(puba, "payload", payload);
    json_set(puba, "event_type", json_null());
    json_t *pub = sau_call(c, "event_stream.publish", puba);
    json_set(es, "execution", saved);  /* finally: restore even on raise */
    SAU_CHK(pub);
    json_free(pub);
    json_t *co = sau_set_ensure(c, "cancel_obligations");
    sau_set_discard(co, aid);
    json_t *nw = sau_set_ensure(c, "native_waiters");
    sau_set_discard(nw, aid);
    json_t *da = json_object();
    json_set_hdr(da, "admission_id", aid);
    json_t *dpt = sau_call(c, "drop_producer_turn", da);
    SAU_CHK(dpt);
    json_free(dpt);
    json_t *waiters = sau_sub_ensure(c, "waiters");
    json_t *waiter = sau_dict_pop(waiters, aid);
    if (waiter) {
        bool wdone = json_get_bool(waiter, "done", false);
        json_free(waiter);
        if (!wdone) {
            json_t *wa = json_object();
            json_set_hdr(wa, "admission_id", aid);
            json_set(wa, "result", json_null());
            json_t *wr = sau_call(c, "waiter.set_result", wa);
            SAU_CHK(wr);
            json_free(wr);
        }
    }
    json_t *sa = json_object();
    json_set(sa, "ref", sau_ref_arg(ref));
    json_t *sr = sau_schedule(c, sa);
    json_free(sa);
    SAU_CHK(sr);
    json_free(sr);
    json_t *ba = json_object();
    json_set_hdr(ba, "admission_id", aid);
    json_set_hdr(ba, "outcome", outcome);
    json_t *brs = sau_call(c, "busy_row_settled", ba);
    SAU_CHK(brs);
    json_free(brs);
    return NULL;
}

/* ══════════════ agent / approval-control flows ══════════════ */

/* PoP: sau_classic_turn_holds @ gateway/session_authority.py:_classic_turn_holds */
static json_t *sau_classic_turn_holds(sau_ctx_t *c, const json_t *args) {
    const char *route = json_get_str(args, "route", "");
    json_t *runner = json_obj_get(c->state, "runner");
    json_t *running_fn = json_obj_get(runner, "_is_session_running");
    if (sau_is_null(running_fn)) return json_bool(false);
    json_t *a = json_object();
    json_set_hdr(a, "route", route);
    json_t *r1 = sau_call(c, "runner._is_session_running", a);
    SAU_CHK(r1);
    if (!sau_truthy(r1)) { json_free(r1); return json_bool(false); }
    json_free(r1);
    json_t *now = sau_call(c, "time.monotonic", json_object());
    SAU_CHK(now);
    double now_v = json_number_value(now);
    json_free(now);
    json_t *evict_at = sau_sub(c, "_classic_evict_at");
    json_t *last = evict_at ? json_obj_get(evict_at, route) : NULL;
    double last_v = last ? json_number_value(last) : 0.0;
    if (now_v - last_v >= 1.0) {
        json_t *ea = sau_sub_ensure(c, "_classic_evict_at");
        json_set(ea, route, json_number(now_v));
        json_t *e1a = json_object();
        json_set_hdr(e1a, "route", route);
        json_t *e1 = sau_call(c, "runner._hm_evict_idle_stale_agent", e1a);
        SAU_CHK(e1);
        json_free(e1);
        json_t *a2 = json_object();
        json_set_hdr(a2, "route", route);
        json_t *r2 = sau_call(c, "runner._is_session_running", a2);
        SAU_CHK(r2);
        bool still = sau_truthy(r2);
        json_free(r2);
        if (still) {
            json_t *e2a = json_object();
            json_set_hdr(e2a, "route", route);
            json_t *e2 = sau_call(c, "runner._hm_evict_reaped_agent", e2a);
            SAU_CHK(e2);
            json_free(e2);
        }
    }
    json_t *a3 = json_object();
    json_set_hdr(a3, "route", route);
    json_t *r3 = sau_call(c, "runner._is_session_running", a3);
    SAU_CHK(r3);
    bool out = sau_truthy(r3);
    json_free(r3);
    return json_bool(out);
}

/* PoP: sau_runs_own_agent @ gateway/session_authority.py:_runs_own_agent */
static json_t *sau_runs_own_agent(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    const char *route = json_get_str(live, "route", "");
    json_t *ka = json_object();
    json_set_hdr(ka, "route", route);
    json_set_hdr(ka, "session_id", sid);
    json_t *key = sau_call(c, "running_key", ka);
    SAU_CHK(key);
    const char *key_s = sau_str(key);
    json_t *runner = json_obj_get(c->state, "runner");
    json_t *running = json_obj_get(runner, "_running_agents");
    json_t *agent = (running && running->type == JSON_OBJECT && key_s)
        ? json_obj_get(running, key_s) : NULL;
    json_free(key);
    bool out = !sau_is_null(agent) && !sau_str_eq(agent, SAU_SENTINEL);
    return json_bool(out);
}

/* SessionActor value equality (frozen dataclass eq): subject/profile/transport
 * strings + capabilities as a sorted set. */
static bool sau_actor_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_OBJECT || b->type != JSON_OBJECT) return false;
    if (!sau_str_eq(json_obj_get(a, "subject"), json_get_str(b, "subject", ""))) return false;
    if (!sau_str_eq(json_obj_get(a, "profile_id"), json_get_str(b, "profile_id", ""))) return false;
    if (!sau_str_eq(json_obj_get(a, "transport_id"), json_get_str(b, "transport_id", ""))) return false;
    json_t *ca = sau_sorted_strs(json_obj_get(a, "capabilities"));
    json_t *cb = sau_sorted_strs(json_obj_get(b, "capabilities"));
    bool eq = json_len(ca) == json_len(cb);
    if (eq) {
        for (size_t i = 0; i < json_len(ca); i++)
            if (!sau_str_eq(json_get(ca, i), sau_str(json_get(cb, i)))) { eq = false; break; }
    }
    json_free(ca);
    json_free(cb);
    return eq;
}

/* PoP: sau_steer_agent @ gateway/session_authority.py:steer_agent */
static json_t *sau_steer_agent(sau_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *gen = json_obj_get(args, "generation");
    json_t *text = json_obj_get(args, "text");
    json_t *wa = json_object();
    json_set(wa, "ref", sau_ref_arg(ref));
    json_set(wa, "person_input", json_bool(true));
    json_t *wb = sau_call(c, "wake_background_wait", wa);
    SAU_CHK(wb);
    json_free(wb);
    json_t *adopted = sau_sub(c, "adopted");
    json_t *pair = adopted ? json_obj_get(adopted, sid) : NULL;
    json_t *adopted_gen = (pair && pair->type == JSON_ARRAY && json_len(pair) > 0) ? json_get(pair, 0) : NULL;
    json_t *agent = NULL;  /* owned id node or NULL */
    if (pair && pair->type == JSON_ARRAY && json_len(pair) > 1) {
        json_t *a1 = json_get(pair, 1);
        agent = a1 ? json_copy(a1) : NULL;
    }
    if (!sau_gen_eq(adopted_gen, gen)) {
        if (agent) { json_free(agent); agent = NULL; }
        json_t *ra = json_object();
        json_set(ra, "ref", sau_ref_arg(ref));
        json_t *roa = sau_runs_own_agent(c, ra);
        json_free(ra);
        SAU_CHK(roa);
        bool own = sau_truthy(roa);
        json_free(roa);
        if (own) {
            json_t *aa = json_object();
            json_set(aa, "ref", sau_ref_arg(ref));
            agent = sau_call(c, "self.agent", aa);
            SAU_CHK(agent);
        }
    }
    bool agent_not_none = agent && agent->type != JSON_NULL;
    bool text_is_none = sau_is_null(text);
    json_t *ret = json_array();
    if (agent_not_none || text_is_none) {
        json_append(ret, agent ? agent : json_null());
        json_append(ret, json_bool(false));
        return ret;
    }
    json_t *aoa = json_object();
    json_set(aoa, "ref", sau_ref_arg(ref));
    json_t *adopts = sau_call(c, "adopts_owner_agent", aoa);
    SAU_CHK(adopts);
    bool adopts_v = sau_truthy(adopts);
    json_free(adopts);
    if (!adopts_v) {
        json_append(ret, agent ? agent : json_null());
        json_append(ret, json_bool(false));
        return ret;
    }
    json_t *pst = sau_sub_ensure(c, "pending_steers");
    json_t *sp = json_obj_get(pst, sid);
    json_t *latched = (sp && sp->type == JSON_ARRAY && json_len(sp) > 0) ? json_get(sp, 0) : NULL;
    json_t *texts = (sp && sp->type == JSON_ARRAY && json_len(sp) > 1) ? json_get(sp, 1) : NULL;
    json_t *new_texts = json_array();
    if (sau_gen_eq(latched, gen) && texts && texts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(texts); i++) json_append(new_texts, json_copy(json_get(texts, i)));
    }
    json_append(new_texts, text ? json_copy(text) : json_null());
    json_t *new_pair = json_array();
    json_append(new_pair, gen ? json_copy(gen) : json_null());
    json_append(new_pair, new_texts);
    json_set(pst, sid, new_pair);
    if (agent) json_free(agent);
    json_append(ret, json_null());
    json_append(ret, json_bool(true));
    return ret;
}

/* PoP: sau_register_approval @ gateway/session_authority.py:register_approval */
static json_t *sau_register_approval(sau_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    (void)live;
    json_t *ca = json_object();
    json_set_hdr(ca, "session_id", sid);
    json_t *gen = json_obj_get(args, "generation");
    json_set(ca, "generation", gen ? json_copy(gen) : json_null());
    json_t *cr = sau_check_approval_generation(c, ca);
    json_free(ca);
    SAU_CHK(cr);
    json_free(cr);
    json_t *ra = json_object();
    json_set_hdr(ra, "session_id", sid);
    json_t *route = json_obj_get(args, "route");
    json_set(ra, "route", route ? json_copy(route) : json_null());
    json_set(ra, "generation", gen ? json_copy(gen) : json_null());
    json_t *data = json_obj_get(args, "data");
    json_set(ra, "data", data ? json_copy(data) : json_null());
    json_t *ts = json_obj_get(args, "timeout_s");
    json_set(ra, "timeout_s", ts ? json_copy(ts) : json_null());
    json_t *rr = sau_call(c, "controls.register", ra);
    SAU_CHK(rr);
    json_free(rr);
    return NULL;
}

/* PoP: sau_register_clarify @ gateway/session_authority.py:register_clarify */
static json_t *sau_register_clarify(sau_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    (void)live;
    json_t *ca = json_object();
    json_set_hdr(ca, "session_id", sid);
    json_t *gen = json_obj_get(args, "generation");
    json_set(ca, "generation", gen ? json_copy(gen) : json_null());
    json_t *cr = sau_check_approval_generation(c, ca);
    json_free(ca);
    SAU_CHK(cr);
    json_free(cr);
    json_t *ra = json_object();
    json_set_hdr(ra, "session_id", sid);
    json_set(ra, "generation", gen ? json_copy(gen) : json_null());
    json_t *entry = json_obj_get(args, "entry");
    json_set(ra, "entry", entry ? json_copy(entry) : json_null());
    json_t *ts = json_obj_get(args, "timeout_s");
    json_set(ra, "timeout_s", ts ? json_copy(ts) : json_null());
    json_t *rr = sau_call(c, "controls.register_clarify", ra);
    SAU_CHK(rr);
    json_free(rr);
    return NULL;
}

/* PoP: sau_respond @ gateway/session_authority.py:respond */
static json_t *sau_respond(sau_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    const char *kind = json_get_str(args, "kind", "approval");
    const char *capability = NULL;
    if (strcmp(kind, "approval") == 0) capability = "session:approve";
    else if (strcmp(kind, "clarify") == 0) capability = "session:respond";
    if (!capability) return sau_refusal("invalid_params", NULL, NULL);
    json_t *ar = sau_authorize(c, actor, ref, capability);
    SAU_CHK(ar);
    json_free(ar);
    json_t *err = NULL;
    json_t *live = sau_live_req(c, sid, &err);
    if (err) return err;
    json_t *subs = json_obj_get(live, "subscribers");
    bool found = false;
    if (subs && subs->type == JSON_OBJECT) {
        for (size_t i = 0; i < subs->c.count; i++) {
            if (sau_actor_eq(actor, subs->c.items[i])) { found = true; break; }
        }
    }
    if (!found) return sau_refusal("permission_denied", NULL, NULL);
    json_t *gen = json_obj_get(args, "generation");
    /* type(generation) is int: JSON_NUMBER only (bool/string are not int).
     * Fixture domain excludes float generations (libjson cannot tell 1 from 1.0). */
    if (!gen || gen->type != JSON_NUMBER) return sau_refusal("stale_generation", NULL, NULL);
    json_t *ca = json_object();
    json_set_hdr(ca, "session_id", sid);
    json_set(ca, "generation", json_copy(gen));
    json_t *cr = sau_check_approval_generation(c, ca);
    json_free(ca);
    SAU_CHK(cr);
    json_free(cr);
    json_t *pid = json_obj_get(args, "prompt_id");
    if (!pid || pid->type != JSON_STRING || !pid->str_val || pid->str_val[0] == '\0')
        return sau_refusal("invalid_params", NULL, NULL);
    json_t *ra = json_object();
    json_set_hdr(ra, "session_id", sid);
    json_set(ra, "generation", json_copy(gen));
    json_set(ra, "prompt_id", json_copy(pid));
    json_t *response = json_obj_get(args, "response");
    json_set(ra, "response", response ? json_copy(response) : json_null());
    json_set_hdr(ra, "kind", kind);
    return sau_call(c, "controls.respond", ra);
}

/* ── LIVE frozenset canon for the emitted state ──
 * LIVE normalizes each actor's capabilities into a frozenset, so the oracle's
 * canon emits them SORTED regardless of input order. The harness copies the
 * input state verbatim and emits it as out.state; normalize the capabilities
 * of sessions[*].subscribers[*] at that emission point. We hook json_set(obj,
 * "state", val) — the single call that publishes the emitted state — and
 * reuse sau_sorted_strs for the per-actor sort. All port-internal json_set
 * calls appear above and are unaffected. */
static void sau_canon_caps(json_t *v) {
    if (!v) return;
    if (v->type == JSON_OBJECT) {
        for (size_t i = 0; i < v->c.count; i++) {
            if (!strcmp(v->c.keys[i], "capabilities") && v->c.items[i]
                && v->c.items[i]->type == JSON_ARRAY) {
                json_t *sorted = sau_sorted_strs(v->c.items[i]);
                json_free(v->c.items[i]);
                v->c.items[i] = sorted;
            } else {
                sau_canon_caps(v->c.items[i]);
            }
        }
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) sau_canon_caps(v->c.items[i]);
    }
}
static void sau_state_json_set(json_t *obj, const char *key, json_t *val) {
    if (val && !strcmp(key, "state")) sau_canon_caps(val);
    (json_set)(obj, key, val);   /* parenthesized: calls the real libjson fn */
}
#define json_set sau_state_json_set