/*
 * gateway_inbound_flow.c — session-authority + inbound-message FLOW cores of
 * gateway/session_authority.py and gateway/run_inbound.py (hermes-agent).
 *
 * Decision sequences only: every live I/O (store reads, restore seams, owner
 * identity, pairing stores, adapter sends, command resolution, i18n) enters
 * through ONE universal seam: gif_call(ctx, name, args). The harness wires it
 * to a recording stub fed the case's scripted results (in.io); the oracle
 * installs recording monkeypatch stubs emitting the identical event shape
 * {"name", "args", "ret"} from the SAME scripts. Refusals mirror the real
 * AuthorityRefusal (reason / field / detail, _one_line bounds included);
 * GateRefusal / AdmissionReceipt / SessionHandle returns mirror the live
 * dataclass asdict shapes.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and gif_-prefixed (gsp_flow_/grun_flow_ taken).
 *
 * Representation contract (shared with tests/oracle_gateway_inbound_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc), ...} object;
 *   AuthorityRefusal adds reason/field/detail (field/detail _one_line-bound).
 * - Platform / MessageType enum members travel as their .value strings.
 * - Agents / adapters / pairing stores travel as id strings; the oracle wraps
 *   non-empty ids in attribute doubles. Agent method seams carry the id.
 * - logger calls are silent on both sides (not traced).
 * - authority_persistence(self) is a silent handle lookup (not traced); its
 *   store's methods are the seams (store.sessions.get, lineage.lineage, ...).
 * - executing_admission.get() reads state.executing_admission silently.
 * - asyncio.to_thread runs its callable synchronously (no trace of its own).
 * - self._peek_session_state is a STATE accessor: it records the same event
 *   (ret = the state node copy) and returns a borrowed pointer into state so
 *   mutations (update_prompt_pending) persist on both sides.
 * - Scripted restore seams may carry "live": both stubs insert that live
 *   session into sessions[session_id] (the live restore's state effect).
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

/* ── module constants (seeded from the live modules) ── */
/* persistence.authority.vocabulary.STEERING */
#define GIF_STEERING "steering"
/* gateway/config.py:DEFAULT_UNAUTHORIZED_DM_DECLINE_MESSAGE */
#define GIF_DECLINE_DEFAULT \
    "Hi! I'm a personal assistant and can only chat with my owner, so I can't help you directly. Sorry!"
/* run_inbound._SLASH_CONFIRM_CMD_CHOICES */
static const char *GIF_CONFIRM_CMD_CHOICES[][2] = {
    {"approve", "once"}, {"yes", "once"}, {"ok", "once"}, {"confirm", "once"},
    {"always", "always"}, {"remember", "always"},
    {"cancel", "cancel"}, {"no", "cancel"}, {"deny", "cancel"}, {"nevermind", "cancel"},
};
#define GIF_N_CMD_CHOICES (sizeof(GIF_CONFIRM_CMD_CHOICES) / sizeof(GIF_CONFIRM_CMD_CHOICES[0]))
/* run_inbound._SLASH_CONFIRM_TEXT_INPUT_KEYS */
static const char *GIF_CONFIRM_TEXT_KEYS[][2] = {
    {"confirm_once", "once"}, {"confirm_always", "always"}, {"confirm_cancel", "cancel"},
};
#define GIF_N_TEXT_KEYS (sizeof(GIF_CONFIRM_TEXT_KEYS) / sizeof(GIF_CONFIRM_TEXT_KEYS[0]))

/* ── universal seam context ── */
typedef struct gif_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gif_ctx_t;

/* ── basic helpers ── */
static bool gif_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool gif_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool gif_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *gif_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool gif_str_eq(const json_t *v, const char *s) {
    const char *t = gif_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* exceptions caught by `except Exception` (not SystemExit & co) */
static bool gif_is_exception(const json_t *r) {
    if (!gif_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0
        && strcmp(t, "GeneratorExit") != 0;
}

static bool gif_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "TimeoutError", "PermissionError", "FileNotFoundError",
        "FileExistsError", "BlockingIOError", "ConnectionError",
        "ConnectionResetError", "BrokenPipeError", "InterruptedError",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

static bool gif_is_oserror(const json_t *r) {
    if (!gif_is_raise(r)) return false;
    return gif_is_oserror_type(json_get_str(r, "__raise__", ""));
}

static json_t *gif_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* str(KeyError(k)) == repr(k) for the string session ids the fixtures use. */
static json_t *gif_keyerror(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return gif_err("KeyError", buf);
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces
 * (str.split()/join), other non-printables drop, bounded; empty -> None.
 * ASCII fixture strings: bytes >= 0x80 are treated as printable (the live
 * Unicode categories are identity on the fixture corpus).
 */
static char *gif_one_line(const char *value, size_t limit) {
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
    /* drop non-printables (Cc control ranges for ASCII) */
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
static json_t *gif_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = gif_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = gif_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *gif_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return gif_refusal(json_get_str(spec, "reason", ""),
                           gif_str(json_obj_get(spec, "field")),
                           gif_str(json_obj_get(spec, "detail")));
    }
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && gif_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = gif_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return gif_err(type, msg);
}
/* ── string helpers (Python str semantics on the fixture corpus) ── */
static char *gif_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* str.strip(): ASCII whitespace both ends (fixtures are ASCII here). */
static char *gif_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lstrip(chars): drop any leading char in the set. */
static char *gif_lstrip(const char *s, const char *chars) {
    if (!s) s = "";
    while (*s && strchr(chars, *s)) s++;
    return gif_strdup(s);
}

/* str.lower(): ASCII fold (the fixture corpus folds ASCII only). */
static char *gif_lower(const char *s) {
    char *o = gif_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t gif_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* byte offset of the first n codepoints (Python s[:n]). */
static size_t gif_utf8_prefix(const char *s, size_t n) {
    size_t count = 0, i = 0;
    while (s[i] && count < n) {
        if (((unsigned char)s[i] & 0xc0) != 0x80) count++;
        i++;
    }
    /* after the nth lead byte, consume its continuation bytes as well:
     * Python's s[:n] ends at a codepoint boundary, not inside it. */
    while (s[i] && ((unsigned char)s[i] & 0xc0) == 0x80) i++;
    return i;
}

/* f"{message_id}" for the int/str ids the fixtures carry. */
static char *gif_id_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gif_strdup("None");
    if (v->type == JSON_STRING) return gif_strdup(v->str_val);
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return gif_strdup(buf);
    }
    if (v->type == JSON_BOOL) return gif_strdup(v->bool_val ? "True" : "False");
    return gif_strdup("");
}

/* ── seam dispatch ── */
/*
 * One scripted entry: a plain value (reused), or {"__script__": true,
 * "ret": v, "raise": spec, "live": spec}. A missing/exhausted script is a
 * fail-closed RuntimeError on both sides.
 */
static json_t *gif_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return gif_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/*
 * The restore seams' shared state effect: a scripted "live" spec installs the
 * restored live session into sessions[session_id] — the SAME effect the live
 * restore_* functions have and the oracle's recording stubs apply.
 */
static void gif_apply_live_effect(gif_ctx_t *c, const char *name, const json_t *entry,
                                  const json_t *args) {
    static const char *restore_seams[] = {
        "restore_local_session", "restore_routed_session", "mount_history", "restore_api_session",
    };
    bool is_restore = false;
    for (size_t i = 0; i < sizeof(restore_seams) / sizeof(restore_seams[0]); i++)
        if (strcmp(name, restore_seams[i]) == 0) is_restore = true;
    if (!is_restore || !entry || entry->type != JSON_OBJECT) return;
    json_t *live = json_obj_get(entry, "live");
    if (!live || live->type != JSON_OBJECT) return;
    const char *sid = json_get_str(args, "session_id", NULL);
    if (!sid) return;
    json_t *sessions = json_obj_get(c->state, "sessions");
    if (!sessions || sessions->type != JSON_OBJECT) return;
    json_t *l = json_object();
    json_t *route = json_obj_get(live, "route");
    json_set(l, "route", route ? json_copy(route) : json_null());
    json_t *source = json_obj_get(live, "source");
    json_set(l, "source", source ? json_copy(source) : json_null());
    json_set(l, "settling", json_object());
    json_set(sessions, sid, l);
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *gif_call(gif_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    const json_t *used = entry;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = gif_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = gif_err("RuntimeError", msg);
            used = NULL;
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            used = json_get(entry, idx);
            res = gif_entry_result(used);
        }
    } else {
        res = gif_entry_result(entry);
    }
    if (!gif_is_raise(res)) gif_apply_live_effect(c, name, used, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* ── state helpers ── */
static json_t *gif_sub(gif_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

/* dict.pop(key, None): owned copy out, key removed. */
static json_t *gif_dict_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    if (!v) return NULL;
    json_t *copy = json_copy(v);
    json_obj_del(obj, key);
    return copy;
}

static bool gif_dict_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* self._peek_session_state: a STATE accessor. Records the identical event the
 * oracle's recorder emits (ret = the state node copy) and returns a BORROWED
 * pointer so update_prompt_pending writes persist into out.state. */
static json_t *gif_peek_state(gif_ctx_t *c, const char *key) {
    json_t *states = gif_sub(c, "session_states");
    json_t *node = (states && key) ? json_obj_get(states, key) : NULL;
    json_t *a = json_object();
    json_set_hdr(a, "key", key ? key : "");
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._peek_session_state");
    json_set(ev, "args", a);
    json_set(ev, "ret", node ? json_copy(node) : json_null());
    json_append(c->trace, ev);
    return node;
}

/* capabilities membership (actor.capabilities is a JSON array of strings). */
static bool gif_caps_has(const json_t *caps, const char *cap) {
    if (!caps || caps->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(caps); i++)
        if (gif_str_eq(json_get(caps, i), cap)) return true;
    return false;
}

/* source.platform == Platform.<X> on the .value strings fixtures carry. */
static bool gif_platform_is(const json_t *source, const char *value) {
    if (!source || source->type != JSON_OBJECT) return false;
    return gif_str_eq(json_obj_get(source, "platform"), value);
}

/* Propagate a raise result unless it is NULL (success). */
#define GIF_CHK(v) do { if (gif_is_raise(v)) return (v); } while (0)
/* ══════════════ gateway/session_authority.py ports ══════════════ */

/* PoP: gif_admission_row @ gateway/session_authority.py:admission_row */
static json_t *gif_admission_row(gif_ctx_t *c, const json_t *args) {
    (void)c;
    json_t *record = json_obj_get(args, "record");
    if (gif_is_null(record)) return NULL;  /* None stays None */
    json_t *row = json_copy(record);       /* asdict(record) */
    json_obj_del(row, "queued_owner_epoch");
    return row;
}

/* PoP: gif_require_admission_open @ gateway/session_authority.py:_require_admission_open */
static json_t *gif_require_admission_open(gif_ctx_t *c, const json_t *args) {
    if (gif_truthy(json_obj_get(c->state, "retiring")))
        return gif_refusal("runtime_draining", NULL, NULL);
    json_t *runner = json_obj_get(c->state, "runner");
    if (gif_truthy(json_obj_get(runner, "_draining"))) {
        bool queue_only = json_get_bool(args, "queue_only", false);
        /* `queue_only and restart_wait_open(...)`: the seam runs only when
         * queue_only is True (short-circuit). */
        bool open = false;
        if (queue_only) {
            json_t *r = gif_call(c, "restart_wait_open", json_object());
            GIF_CHK(r);
            open = gif_truthy(r);
            json_free(r);
        }
        if (!(queue_only && open))
            return gif_refusal("runtime_draining", NULL, NULL);
    }
    return NULL;
}

/* GateRefusal(exc.reason, field=exc.field, detail=exc.detail) asdict shape. */
static json_t *gif_gate_from_refusal(json_t *r) {
    json_t *g = json_object();
    json_t *reason = json_obj_get(r, "reason");
    json_set(g, "reason", reason ? json_copy(reason) : json_null());
    json_t *field = json_obj_get(r, "field");
    json_set(g, "field", field ? json_copy(field) : json_null());
    json_t *detail = json_obj_get(r, "detail");
    json_set(g, "detail", detail ? json_copy(detail) : json_null());
    json_free(r);
    return g;
}

/* PoP: gif_gate_refusal @ gateway/session_authority.py:gate_refusal */
static json_t *gif_gate_refusal(gif_ctx_t *c, const json_t *args) {
    bool has_check = json_get_bool(args, "check", false);
    bool queue_only = json_get_bool(args, "queue_only", false);
    json_t *a = json_object();
    json_set(a, "queue_only", json_bool(queue_only));
    json_t *r = gif_require_admission_open(c, a);
    if (gif_is_raise(r)) {
        if (gif_str_eq(json_obj_get(r, "__raise__"), "AuthorityRefusal"))
            return gif_gate_from_refusal(r);
        return r;
    }
    if (has_check) {  /* check is not None */
        json_t *cr = gif_call(c, "check", json_object());
        if (gif_is_raise(cr)) {
            if (gif_str_eq(json_obj_get(cr, "__raise__"), "AuthorityRefusal"))
                return gif_gate_from_refusal(cr);
            return cr;
        }
        json_free(cr);
    }
    return NULL;
}

/* PoP: gif_logical_owner @ gateway/session_authority.py:logical_owner */
static json_t *gif_logical_owner(gif_ctx_t *c, const json_t *args) {
    json_t *sid = json_obj_get(args, "session_id");
    if (!gif_truthy(sid)) return sid ? json_copy(sid) : json_null();
    json_t *a = json_object();
    json_set(a, "session_id", json_copy(sid));
    json_t *lin = gif_call(c, "lineage.lineage", a);
    GIF_CHK(lin);
    json_t *out;
    if (gif_truthy(lin) && lin->type == JSON_ARRAY && json_len(lin) > 0)
        out = json_copy(json_get(lin, 0));
    else if (lin && lin->type == JSON_STRING && lin->str_val && lin->str_val[0]) {
        /* Python indexes a string too: lineage[0] is its first character. */
        char one[2] = {lin->str_val[0], 0};
        out = json_string(one);
    } else
        out = json_copy(sid);
    json_free(lin);
    return out;
}

/* PoP: gif_physical_target @ gateway/session_authority.py:physical_target */
static json_t *gif_physical_target(gif_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    json_t *sid = json_obj_get(ref, "session_id");
    json_t *a = json_object();
    json_set(a, "session_id", sid ? json_copy(sid) : json_null());
    json_t *tip = gif_call(c, "lineage.tip", a);
    GIF_CHK(tip);
    json_t *out = gif_truthy(tip) ? tip : (sid ? json_copy(sid) : json_null());
    if (out != tip) json_free(tip);
    return out;
}

/* PoP: gif_current_owner @ gateway/session_authority.py:_current_owner */
static json_t *gif_current_owner(gif_ctx_t *c, const json_t *args) {
    json_t *sid = json_obj_get(args, "session_id");
    const char *sid_s = gif_str(sid);
    json_t *rotated = gif_sub(c, "rotated_owners");
    json_t *route = sid_s ? json_obj_get(rotated, sid_s) : NULL;
    if (gif_is_null(route)) return sid ? json_copy(sid) : json_null();
    json_t *a = json_object();
    json_set(a, "route", json_copy(route));
    json_t *entry = gif_call(c, "runner.session_store.lookup_by_session_key", a);
    GIF_CHK(entry);
    json_t *current;
    if (gif_is_null(entry)) {
        current = sid ? json_copy(sid) : json_null();
    } else {
        json_t *a2 = json_object();
        json_t *esid = json_obj_get(entry, "session_id");
        json_set(a2, "session_id", esid ? json_copy(esid) : json_null());
        current = gif_logical_owner(c, a2);
    }
    json_free(entry);
    if (gif_is_raise(current)) return current;
    const char *cur_s = gif_str(current);
    json_t *sessions = gif_sub(c, "sessions");
    if (cur_s && gif_dict_has(sessions, cur_s)) return current;
    json_free(current);
    return sid ? json_copy(sid) : json_null();
}

/* PoP: gif_receipt @ gateway/session_authority.py:_receipt */
static json_t *gif_receipt(gif_ctx_t *c, const json_t *args) {
    json_t *row = json_obj_get(args, "row");
    const char *status = json_get_str(row, "status", "");
    json_t *outcome = json_obj_get(row, "outcome");
    bool steering = gif_str_eq(outcome, GIF_STEERING);
    if (steering) { status = "started"; outcome = NULL; }
    json_t *oe = json_obj_get(row, "owner_epoch");
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_t *out = json_object();
    json_t *aid = json_obj_get(row, "admission_id");
    json_set(out, "admission_id", aid ? json_copy(aid) : json_null());
    json_t *ref = json_object();
    json_t *pid = json_obj_get(c->state, "profile_id");
    json_set(ref, "profile_id", pid ? json_copy(pid) : json_null());
    json_t *tsid = json_obj_get(row, "target_session_id");
    json_set(ref, "session_id", tsid ? json_copy(tsid) : json_null());
    json_set(out, "ref", ref);
    json_t *seq = json_obj_get(row, "seq");
    json_set(out, "sequence", seq ? json_copy(seq) : json_null());
    json_set_hdr(out, "status", status);
    json_set(out, "outcome", outcome ? json_copy(outcome) : json_null());
    json_set(out, "authority_epoch", gif_truthy(oe) ? json_copy(oe)
                                 : (epoch ? json_copy(epoch) : json_null()));
    json_t *gen = json_obj_get(row, "generation");
    json_set(out, "execution_generation", gen ? json_copy(gen) : json_null());
    return out;
}

/* SessionAuthority._pending_rows (internal helper of _handle): the event
 * stream's view — a settling claim masks its committed row; masked results
 * sort by seq, unmasked ones keep the store's list order. */
static json_t *gif_pending_rows(gif_ctx_t *c, const char *sid) {
    json_t *a = json_object();
    json_set_hdr(a, "session_id", sid ? sid : "");
    json_t *listed = gif_call(c, "store.admissions.list", a);
    GIF_CHK(listed);
    json_t *rows = json_array();
    if (listed && listed->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(listed); i++) {
            json_t *row = json_copy(json_get(listed, i));
            json_obj_del(row, "queued_owner_epoch");  /* admission_row */
            json_append(rows, row);
        }
    } else if (listed && listed->type == JSON_OBJECT) {
        /* a one-row script arrives as the row itself, not a list of rows */
        json_t *row = json_copy(listed);
        json_obj_del(row, "queued_owner_epoch");
        json_append(rows, row);
    }
    json_free(listed);
    json_t *sessions = gif_sub(c, "sessions");
    json_t *live = sid ? json_obj_get(sessions, sid) : NULL;
    json_t *masked = live ? json_obj_get(live, "settling") : NULL;
    if (!gif_truthy(masked)) return rows;
    /* rows = {admission_id: row}; rows.update(masked); sorted by seq */
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
    /* stable insertion sort by seq over the dict values */
    size_t n = by_id->c.count;
    json_t **vals = malloc(n * sizeof(json_t *));
    if (!vals) { json_free(by_id); return json_array(); }
    for (size_t i = 0; i < n; i++) vals[i] = by_id->c.items[i];
    for (size_t i = 1; i < n; i++) {
        json_t *v = vals[i];
        double seq = json_get_num(v, "seq", 0);
        size_t j = i;
        while (j > 0 && json_get_num(vals[j - 1], "seq", 0) > seq) {
            vals[j] = vals[j - 1];
            j--;
        }
        vals[j] = v;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(vals[i]));
    free(vals);
    json_free(by_id);
    return out;
}

/* PoP: gif_handle @ gateway/session_authority.py:_handle */
static json_t *gif_handle(gif_ctx_t *c, const json_t *args) {
    json_t *ref = json_obj_get(args, "ref");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *a = json_object();
    json_set_hdr(a, "session_id", sid);
    json_t *row = gif_call(c, "store.sessions.get", a);
    GIF_CHK(row);
    if (gif_is_null(row)) return gif_refusal("not_found", NULL, NULL);
    json_t *pending = gif_pending_rows(c, sid);
    if (gif_is_raise(pending)) { json_free(row); return pending; }
    const char *state = "idle";
    bool any_started = false;
    for (size_t i = 0; i < json_len(pending); i++) {
        const char *st = json_get_str(json_get(pending, i), "status", "");
        if (strcmp(st, "unknown") == 0) { state = "unknown"; break; }
        if (strcmp(st, "started") == 0) any_started = true;
    }
    if (strcmp(state, "unknown") != 0 && any_started) state = "running";
    json_free(pending);
    json_t *out = json_object();
    json_t *r = json_object();
    json_t *pid = json_obj_get(ref, "profile_id");
    json_set(r, "profile_id", pid ? json_copy(pid) : json_null());
    json_set_hdr(r, "session_id", sid);
    json_set(out, "ref", r);
    json_t *iid = json_obj_get(c->state, "instance_id");
    json_set(out, "instance_id", iid ? json_copy(iid) : json_null());
    json_t *epoch = json_obj_get(c->state, "epoch");
    json_set(out, "authority_epoch", epoch ? json_copy(epoch) : json_null());
    json_t *rev = json_obj_get(row, "runtime_revision");
    json_set(out, "revision", rev ? json_copy(rev) : json_null());
    json_t *gen = json_obj_get(row, "runtime_generation");
    json_set(out, "execution_generation", gen ? json_copy(gen) : json_null());
    json_set_hdr(out, "execution_state", state);
    json_free(row);
    return out;
}
/* PoP: gif_authorize @ gateway/session_authority.py:authorize */
static json_t *gif_authorize(gif_ctx_t *c, const json_t *args) {
    json_t *actor = json_obj_get(args, "actor");
    json_t *ref = json_obj_get(args, "ref");
    const char *capability = json_get_str(args, "capability", "");
    const char *sid = json_get_str(ref, "session_id", "");
    json_t *r = gif_call(c, "require_serving", json_object());
    GIF_CHK(r);
    json_free(r);
    json_t *pid = json_obj_get(c->state, "profile_id");
    if (!gif_str_eq(json_obj_get(actor, "profile_id"), gif_str(pid))
        || !gif_str_eq(json_obj_get(ref, "profile_id"), gif_str(pid)))
        return gif_refusal("profile_mismatch", NULL, NULL);
    json_t *caps = json_obj_get(actor, "capabilities");
    if (!gif_caps_has(caps, capability))
        return gif_refusal("permission_denied", NULL, NULL);
    json_t *sessions = gif_sub(c, "sessions");
    if (!gif_dict_has(sessions, sid)) {
        json_t *a = json_object();
        json_set_hdr(a, "session_id", sid);
        json_t *row = gif_call(c, "store.sessions.get", a);
        GIF_CHK(row);
        if (gif_is_null(row)) return gif_refusal("not_found", NULL, NULL);
        json_t *a2 = json_object();
        json_set_hdr(a2, "session_id", sid);
        json_t *marks = gif_call(c, "store.bindings.binding_marks", a2);
        if (gif_is_raise(marks)) { json_free(row); return marks; }
        /* str(row.chat_id or '').startswith('local-') */
        json_t *chat = json_obj_get(row, "chat_id");
        const char *chat_s = gif_truthy(chat) ? gif_str(chat) : "";
        bool local_chat = chat_s && strncmp(chat_s, "local-", 6) == 0;
        bool restore_local = gif_truthy(json_obj_get(marks, "policy"))
            || gif_truthy(json_obj_get(marks, "legacy")) || local_chat;
        json_free(marks);
        json_free(row);
        if (restore_local) {
            json_t *a3 = json_object();
            json_set_hdr(a3, "session_id", sid);
            json_t *rr = gif_call(c, "restore_local_session", a3);
            GIF_CHK(rr);
            json_free(rr);
        } else {
            json_t *a3 = json_object();
            json_set_hdr(a3, "session_id", sid);
            json_t *rr = gif_call(c, "restore_routed_session", a3);
            GIF_CHK(rr);
            bool routed_none = gif_is_null(rr);
            json_free(rr);
            bool mounted = false;
            if (routed_none) {
                json_t *a4 = json_object();
                json_set(a4, "actor", json_copy(actor));
                json_set_hdr(a4, "session_id", sid);
                json_t *m = gif_call(c, "mount_history", a4);
                GIF_CHK(m);
                mounted = gif_truthy(m);
                json_free(m);
            }
            if (routed_none && !mounted) {
                json_t *a5 = json_object();
                json_set_hdr(a5, "session_id", sid);
                json_t *ra = gif_call(c, "restore_api_session", a5);
                GIF_CHK(ra);
                json_free(ra);
            }
        }
    }
    /* self.sessions[ref.session_id] — a restore that installed nothing is a
     * KeyError here, exactly as live. */
    sessions = gif_sub(c, "sessions");
    json_t *live = gif_dict_has(sessions, sid) ? json_obj_get(sessions, sid) : NULL;
    if (!live) return gif_keyerror(sid);
    json_t *a6 = json_object();
    json_set_hdr(a6, "session_id", sid);
    json_t *hm = gif_call(c, "is_history_mount", a6);
    GIF_CHK(hm);
    bool is_mount = gif_truthy(hm);
    json_free(hm);
    if (is_mount) {
        json_t *a7 = json_object();
        json_set(a7, "actor", json_copy(actor));
        json_set_hdr(a7, "session_id", sid);
        json_set_hdr(a7, "capability", capability);
        json_t *ha = gif_call(c, "require_history_access", a7);
        GIF_CHK(ha);
        json_free(ha);
    }
    json_t *source = json_obj_get(live, "source");
    if (!gif_is_null(source) && gif_platform_is(source, "local")) {
        json_t *a8 = json_object();
        json_t *uid = json_obj_get(source, "user_id");
        json_set(a8, "a", uid ? json_copy(uid) : json_null());
        json_t *subj = json_obj_get(actor, "subject");
        json_set(a8, "b", subj ? json_copy(subj) : json_null());
        json_t *so = gif_call(c, "same_owner", a8);
        GIF_CHK(so);
        bool same = gif_truthy(so);
        json_free(so);
        if (!same && !gif_caps_has(caps, "session:operator"))
            return gif_refusal("permission_denied", NULL, NULL);
    }
    json_t *a9 = json_object();
    json_set_hdr(a9, "session_id", sid);
    json_t *tr = gif_call(c, "touch_restored", a9);
    GIF_CHK(tr);
    json_free(tr);
    return NULL;
}

/* PoP: gif_adopt_agent @ gateway/session_authority.py:adopt_agent */
static json_t *gif_adopt_agent(gif_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *generation = json_obj_get(args, "generation");
    double gen = generation ? json_number_value(generation) : 0.0;
    json_t *agent = json_obj_get(args, "agent");
    const char *agent_id = gif_is_null(agent) ? NULL : json_get_str(agent, "id", NULL);
    bool has_clear = !gif_is_null(agent) && json_get_bool(agent, "has_clear", false);
    /* live = self.sessions.get(session_id); the stream lock orders the latch
     * writes — a no-op in the single-threaded harness on both sides. */
    json_t *adopted = gif_sub(c, "adopted");
    if (!adopted) { adopted = json_object(); json_set(c->state, "adopted", adopted); }
    json_t *pair = json_array();
    json_append(pair, generation ? json_copy(generation) : json_null());
    json_append(pair, agent_id ? json_string(agent_id) : json_null());
    json_set(adopted, sid, pair);
    /* delivered_stops.pop(session_id, generation) != generation -> clear() */
    json_t *ds = gif_sub(c, "delivered_stops");
    json_t *prev = gif_dict_pop(ds, sid);
    double prev_gen = prev ? json_number_value(prev) : gen;
    json_free(prev);
    if (prev_gen != gen && has_clear) {
        json_t *a = json_object();
        json_set_hdr(a, "agent", agent_id);
        json_t *r = gif_call(c, "agent.clear_interrupt", a);
        GIF_CHK(r);
        json_free(r);
    }
    /* pending_stops.get(session_id) == generation -> deliver the latched Stop */
    json_t *ps = gif_sub(c, "pending_stops");
    json_t *latched_stop = ps ? json_obj_get(ps, sid) : NULL;
    if (latched_stop && latched_stop->type == JSON_NUMBER && json_number_value(latched_stop) == gen) {
        json_obj_del(ps, sid);
        json_t *a = json_object();
        json_set(a, "agent", agent_id ? json_string(agent_id) : json_null());
        json_t *r = gif_call(c, "request_hard_interrupt", a);
        GIF_CHK(r);
        json_free(r);
    }
    /* pending_steers.pop(session_id, (None, ())): replay only this generation's */
    json_t *pst = gif_sub(c, "pending_steers");
    json_t *steer_pair = gif_dict_pop(pst, sid);
    if (steer_pair && steer_pair->type == JSON_ARRAY && json_len(steer_pair) == 2) {
        json_t *latched = json_get(steer_pair, 0);
        json_t *texts = json_get(steer_pair, 1);
        if (latched && latched->type == JSON_NUMBER && json_number_value(latched) == gen
            && texts && texts->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(texts); i++) {
                if (!agent_id) {
                    json_free(steer_pair);
                    return gif_err("AttributeError", "'NoneType' object has no attribute 'steer'");
                }
                json_t *a = json_object();
                json_set_hdr(a, "agent", agent_id);
                json_t *text = json_get(texts, i);
                json_set(a, "text", text ? json_copy(text) : json_null());
                json_t *r = gif_call(c, "agent.steer", a);
                if (gif_is_raise(r)) { json_free(steer_pair); return r; }
                json_free(r);
            }
        }
    }
    json_free(steer_pair);
    return NULL;
}

/* PoP: gif_publish_execution @ gateway/session_authority.py:publish_execution */
static json_t *gif_publish_execution(gif_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    json_t *generation = json_obj_get(args, "generation");
    json_t *event_type = json_obj_get(args, "event_type");
    json_t *payload = json_obj_get(args, "payload");
    json_t *sessions = gif_sub(c, "sessions");
    if (!gif_dict_has(sessions, sid)) return json_bool(false);
    json_t *executing = gif_sub(c, "executing");
    json_t *cur = executing ? json_obj_get(executing, sid) : NULL;
    if (!cur || cur->type != JSON_NUMBER || !generation
        || json_number_value(cur) != json_number_value(generation))
        return json_bool(false);
    json_t *a1 = json_object();
    json_set_hdr(a1, "session_id", sid);
    json_set(a1, "generation", generation ? json_copy(generation) : json_null());
    json_set(a1, "event_type", event_type ? json_copy(event_type) : json_null());
    json_set(a1, "payload", payload ? json_copy(payload) : json_null());
    json_t *np = gif_call(c, "event_stream.note_progress", a1);
    GIF_CHK(np);
    json_free(np);
    json_t *a2 = json_object();
    json_set_hdr(a2, "session_id", sid);
    json_set(a2, "payload", payload ? json_copy(payload) : json_null());
    json_set(a2, "event_type", event_type ? json_copy(event_type) : json_null());
    json_t *delivered = gif_call(c, "event_stream.publish", a2);
    GIF_CHK(delivered);
    json_t *a3 = json_object();
    json_set_hdr(a3, "session_id", sid);
    json_set(a3, "event_type", event_type ? json_copy(event_type) : json_null());
    json_set(a3, "payload", payload ? json_copy(payload) : json_null());
    json_t *observed = gif_call(c, "publish_api_event", a3);
    if (gif_is_raise(observed)) { json_free(delivered); return observed; }
    bool out = gif_truthy(delivered) || gif_truthy(observed);
    json_free(delivered);
    json_free(observed);
    return json_bool(out);
}
/* ══════════════ gateway/run_inbound.py ports ══════════════ */

/* The Discord triggering note (em dash is UTF-8 e2 80 94). */
static char *gif_discord_note(const char *id_str) {
    const char *pre = "[Triggering message id: `";
    const char *post = "` \xe2\x80\x94 use as `message_id` for reply/react/pin "
                       "via the discord tools.]";
    size_t n = strlen(pre) + strlen(id_str) + strlen(post) + 1;
    char *o = malloc(n);
    if (o) snprintf(o, n, "%s%s%s", pre, id_str, post);
    return o;
}

/* PoP: gif_discord_triggering_note @ gateway/run_inbound.py:discord_triggering_note */
static json_t *gif_discord_triggering_note(gif_ctx_t *c, const json_t *args) {
    (void)c;
    char *id = gif_id_str(json_obj_get(args, "message_id"));
    char *note = gif_discord_note(id ? id : "");
    free(id);
    json_t *out = json_string(note ? note : "");
    free(note);
    return out;
}

/* PoP: gif_strip_discord_triggering_note @ gateway/run_inbound.py:strip_discord_triggering_note */
static json_t *gif_strip_discord_triggering_note(gif_ctx_t *c, const json_t *args) {
    (void)c;
    json_t *event = json_obj_get(args, "event");
    json_t *mid = json_obj_get(event, "message_id");
    json_t *text = json_obj_get(args, "message_text");
    if (!gif_truthy(mid) || !text || text->type != JSON_STRING)
        return text ? json_copy(text) : json_null();
    char *id = gif_id_str(mid);
    char *note = gif_discord_note(id ? id : "");
    free(id);
    size_t nl = note ? strlen(note) : 0;
    char *prefix = malloc(nl + 3);
    json_t *out = NULL;
    if (prefix) {
        memcpy(prefix, note ? note : "", nl);
        prefix[nl] = '\n'; prefix[nl + 1] = '\n'; prefix[nl + 2] = '\0';
        if (strncmp(text->str_val, prefix, nl + 2) == 0)
            out = json_string(text->str_val + nl + 2);
        free(prefix);
    }
    free(note);
    if (!out) out = json_copy(text);
    return out;
}

/* PoP: gif_estop_turn_allowed @ gateway/run_inbound.py:_hm_estop_turn_allowed */
static json_t *gif_estop_turn_allowed(gif_ctx_t *c, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *source = json_obj_get(args, "source");
    /* with suppress(Exception): recognized slash commands pass */
    json_t *cmd = gif_call(c, "event.get_command", json_object());
    if (gif_is_raise(cmd)) {
        json_free(cmd);
    } else {
        if (gif_truthy(cmd)) {
            json_t *a = json_object();
            json_set(a, "cmd", json_copy(cmd));
            json_t *def = gif_call(c, "resolve_command", a);
            if (gif_is_raise(def)) {
                json_free(def);
            } else {
                bool known = !gif_is_null(def);  /* _resolve_estop_cmd(cmd) is not None */
                json_free(def);
                if (known) { json_free(cmd); return json_bool(true); }
            }
        }
        json_free(cmd);
    }
    /* with suppress(Exception): replies owned by in-flight work pass */
    json_t *a2 = json_object();
    json_set(a2, "source", source ? json_copy(source) : json_null());
    json_t *key = gif_call(c, "self._session_key_for_source", a2);
    if (gif_is_raise(key)) { json_free(key); return json_bool(false); }
    const char *key_s = gif_str(key);
    json_t *st = gif_peek_state(c, key_s);
    if (st) {
        json_t *persistent = json_obj_get(st, "persistent");
        if (gif_truthy(json_obj_get(persistent, "update_prompt_pending"))) {
            json_t *a3 = json_object();
            json_set(a3, "event", event ? json_copy(event) : json_null());
            json_t *refused = gif_call(c, "self._hm_update_reply_refused", a3);
            if (gif_is_raise(refused)) { json_free(refused); json_free(key); return json_bool(false); }
            bool ref = gif_truthy(refused);
            json_free(refused);
            if (!ref) { json_free(key); return json_bool(true); }
        }
    }
    json_t *a4 = json_object();
    json_set_hdr(a4, "key", key_s ? key_s : "");
    json_t *running = gif_call(c, "self._is_session_running", a4);
    if (gif_is_raise(running)) { json_free(running); json_free(key); return json_bool(false); }
    if (gif_truthy(running)) { json_free(running); json_free(key); return json_bool(true); }
    json_free(running);
    json_t *a5 = json_object();
    json_set_hdr(a5, "key", key_s ? key_s : "");
    json_t *pc = gif_call(c, "slash_confirm.get_pending", a5);
    if (gif_is_raise(pc)) { json_free(pc); json_free(key); return json_bool(false); }
    if (gif_truthy(pc)) { json_free(pc); json_free(key); return json_bool(true); }
    json_free(pc);
    json_t *a6 = json_object();
    json_set_hdr(a6, "key", key_s ? key_s : "");
    json_t *ba = gif_call(c, "approval.has_blocking_approval", a6);
    if (gif_is_raise(ba)) { json_free(ba); json_free(key); return json_bool(false); }
    bool blocked = gif_truthy(ba);
    json_free(ba);
    json_free(key);
    return json_bool(blocked);
}

/* PoP: gif_estop_gate @ gateway/run_inbound.py:_hm_estop_gate */
static json_t *gif_estop_gate(gif_ctx_t *c, const json_t *args) {
    if (json_get_bool(args, "is_internal", false)) return NULL;
    json_t *notice = gif_call(c, "agent.estop.paused_reply", json_object());
    if (gif_is_raise(notice)) return notice;
    if (gif_is_null(notice)) return NULL;  /* notice is None: allowed NOT called */
    json_t *allowed = gif_estop_turn_allowed(c, args);
    if (gif_is_raise(allowed)) { json_free(notice); return allowed; }
    bool pass = gif_truthy(allowed);
    json_free(allowed);
    if (pass) { json_free(notice); return NULL; }
    json_t *event = json_obj_get(args, "event");
    if (gif_truthy(json_obj_get(event, "_heartbeat_session_id"))) {
        json_free(notice);
        return json_string("");
    }
    return notice;
}
/* PoP: gif_update_prompt_reply @ gateway/run_inbound.py:_hm_update_prompt_reply */
static json_t *gif_update_prompt_reply(gif_ctx_t *c, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    const char *key = json_get_str(args, "key", "");
    json_t *st = gif_peek_state(c, key);
    json_t *persistent = st ? json_obj_get(st, "persistent") : NULL;
    if (!st || !gif_truthy(json_obj_get(persistent, "update_prompt_pending")))
        return NULL;
    json_t *a0 = json_object();
    json_set(a0, "event", event ? json_copy(event) : json_null());
    json_t *refused = gif_call(c, "self._hm_update_reply_refused", a0);
    GIF_CHK(refused);
    if (gif_truthy(refused)) { json_free(refused); return NULL; }
    json_free(refused);
    json_t *cmd = gif_call(c, "event.get_command", json_object());
    GIF_CHK(cmd);
    const char *cmd_s = gif_str(cmd);
    const char *response_text = NULL;
    char *resp_buf = NULL;
    const char *recognized = NULL;
    json_t *def = NULL;
    if (cmd_s && (strcmp(cmd_s, "approve") == 0 || strcmp(cmd_s, "yes") == 0)) {
        response_text = "y";
    } else if (cmd_s && (strcmp(cmd_s, "deny") == 0 || strcmp(cmd_s, "no") == 0)) {
        response_text = "n";
    } else {
        if (gif_truthy(cmd)) {
            /* with suppress(Exception): resolve_command + .name read */
            json_t *a = json_object();
            json_set(a, "cmd", json_copy(cmd));
            def = gif_call(c, "resolve_command", a);
            if (gif_is_raise(def)) {
                json_free(def);
                def = NULL;
            } else if (def && def->type == JSON_OBJECT) {
                recognized = gif_str(json_obj_get(def, "name"));
                /* a def without .name raises AttributeError inside the
                 * suppress block: recognized stays None */
            } else if (def && def->type != JSON_NULL) {
                recognized = NULL;
            }
        }
        if (recognized) {
            response_text = "";
        } else {
            json_t *text = json_obj_get(event, "text");
            const char *t = (text && text->type == JSON_STRING) ? text->str_val : "";
            resp_buf = gif_strip(t);
            response_text = resp_buf ? resp_buf : "";
        }
    }
    json_t *out = NULL;
    if (response_text && response_text[0]) {
        json_t *a = json_object();
        json_set_hdr(a, "response_text", response_text);
        json_t *err = gif_call(c, "self._hm_write_update_response", a);
        if (gif_is_raise(err)) { out = err; goto done; }
        if (!gif_is_null(err)) {
            json_t *ta = json_object();
            json_set_hdr(ta, "key", "gateway.update.reply_failed");
            json_set(ta, "error", json_copy(err));
            json_free(err);
            out = gif_call(c, "t", ta);
            goto done;  /* state NOT cleared on a failed write */
        }
        json_free(err);
        json_set(persistent, "update_prompt_pending", json_bool(false));
        /* label = text if len <= 20 else text[:20] + '…' (UTF-8 e2 80 a6) */
        char *label;
        if (gif_utf8_len(response_text) <= 20) {
            label = gif_strdup(response_text);
        } else {
            size_t off = gif_utf8_prefix(response_text, 20);
            label = malloc(off + 4);
            if (label) {
                memcpy(label, response_text, off);
                memcpy(label + off, "\xe2\x80\xa6", 4);
            }
        }
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "gateway.update.reply_sent");
        json_set_hdr(ta, "label", label ? label : "");
        free(label);
        out = gif_call(c, "t", ta);
        goto done;
    }
    if (recognized) {
        json_t *a = json_object();
        json_set_hdr(a, "response_text", "");
        json_t *err = gif_call(c, "self._hm_write_update_response", a);
        if (gif_is_raise(err)) { out = err; goto done; }
        json_free(err);  /* logger.info / logger.warning: silent both sides */
        json_set(persistent, "update_prompt_pending", json_bool(false));
    }
    out = NULL;
done:
    json_free(cmd);
    json_free(def);
    free(resp_buf);
    return out;
}

/* GatewayInboundMixin._slash_confirm_text_choices (internal helper): the
 * free-text spellings, first word wins (dict.setdefault). */
static json_t *gif_slash_confirm_text_choices(gif_ctx_t *c) {
    json_t *choices = json_object();
    for (size_t k = 0; k < GIF_N_TEXT_KEYS; k++) {
        json_t *a = json_object();
        json_set_hdr(a, "input_key", GIF_CONFIRM_TEXT_KEYS[k][0]);
        json_t *words = gif_call(c, "approval_input_words", a);
        if (gif_is_raise(words)) { json_free(choices); return words; }
        if (words && words->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(words); i++) {
                const char *w = gif_str(json_get(words, i));
                if (w && !json_obj_get(choices, w))
                    json_set(choices, w, json_string(GIF_CONFIRM_TEXT_KEYS[k][1]));
            }
        }
        json_free(words);
    }
    return choices;
}

/* PoP: gif_slash_confirm_reply @ gateway/run_inbound.py:_hm_slash_confirm_reply */
static json_t *gif_slash_confirm_reply(gif_ctx_t *c, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    const char *key = json_get_str(args, "key", "");
    json_t *a0 = json_object();
    json_set_hdr(a0, "key", key);
    json_t *pc = gif_call(c, "slash_confirm.get_pending", a0);
    GIF_CHK(pc);
    if (!gif_truthy(pc)) { json_free(pc); return NULL; }
    /* with suppress(Exception): a pending dangerous-command approval wins */
    json_t *a1 = json_object();
    json_set_hdr(a1, "key", key);
    json_t *ba = gif_call(c, "approval.has_blocking_approval", a1);
    if (gif_is_raise(ba)) {
        json_free(ba);
    } else {
        bool blocked = gif_truthy(ba);
        json_free(ba);
        if (blocked) { json_free(pc); return NULL; }
    }
    /* _norm_reply = (event.text or "").strip().lstrip("!/").lower() */
    json_t *text = json_obj_get(event, "text");
    const char *t = (text && text->type == JSON_STRING) ? text->str_val : "";
    char *s1 = gif_strip(t);
    char *s2 = gif_lstrip(s1 ? s1 : "", "!/");
    free(s1);
    char *norm = gif_lower(s2 ? s2 : "");
    free(s2);
    /* cmd choice wins; the text-choices seams run only when it misses */
    json_t *cmd = gif_call(c, "event.get_command", json_object());
    if (gif_is_raise(cmd)) { free(norm); json_free(pc); return cmd; }
    const char *cmd_s = gif_str(cmd);
    const char *choice = NULL;
    if (cmd_s) {
        for (size_t i = 0; i < GIF_N_CMD_CHOICES; i++)
            if (strcmp(cmd_s, GIF_CONFIRM_CMD_CHOICES[i][0]) == 0)
                choice = GIF_CONFIRM_CMD_CHOICES[i][1];
    }
    json_t *tc = NULL;
    if (!choice) {
        tc = gif_slash_confirm_text_choices(c);
        if (gif_is_raise(tc)) { free(norm); json_free(cmd); json_free(pc); return tc; }
        const char *v = norm ? gif_str(json_obj_get(tc, norm)) : NULL;
        choice = v;  /* borrowed from tc; kept alive until used below */
    }
    json_t *out = NULL;
    if (choice) {
        json_t *cid = json_obj_get(pc, "confirm_id");
        const char *cid_s = gif_truthy(cid) ? gif_str(cid) : "";
        json_t *a2 = json_object();
        json_set(a2, "event", event ? json_copy(event) : json_null());
        json_set_hdr(a2, "session_key", key);
        json_set_hdr(a2, "confirm_id", cid_s ? cid_s : "");
        json_t *refusal = gif_call(c, "self._refused_slash_confirm", a2);
        if (gif_is_raise(refusal)) { out = refusal; goto done; }
        if (gif_truthy(refusal)) { out = refusal; goto done; }
        json_free(refusal);
        json_t *a3 = json_object();
        json_set_hdr(a3, "key", key);
        json_set(a3, "confirm_id", cid ? json_copy(cid) : json_null());
        json_set_hdr(a3, "choice", choice);
        json_t *resolved = gif_call(c, "slash_confirm.resolve", a3);
        if (gif_is_raise(resolved)) { out = resolved; goto done; }
        out = gif_truthy(resolved) ? resolved : json_string("");
        if (out != resolved) json_free(resolved);
        goto done;
    }
    json_t *a4 = json_object();
    json_set_hdr(a4, "key", key);
    json_t *cl = gif_call(c, "slash_confirm.clear_if_stale", a4);
    if (gif_is_raise(cl)) { out = cl; goto done; }
    json_free(cl);
    out = NULL;
done:
    free(norm);
    json_free(cmd);
    json_free(tc);
    json_free(pc);
    return out;
}
/* PoP: gif_busy_slash_or_photo @ gateway/run_inbound.py:_hm_busy_slash_or_photo */
static json_t *gif_busy_slash_or_photo(gif_ctx_t *c, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *cmd = gif_call(c, "event.get_command", json_object());
    GIF_CHK(cmd);
    json_t *def = NULL;
    if (gif_truthy(cmd)) {
        json_t *a = json_object();
        json_set(a, "cmd", json_copy(cmd));
        def = gif_call(c, "resolve_command", a);
        if (gif_is_raise(def)) { json_free(cmd); return def; }
    }
    json_free(cmd);
    /* `if _cmd_def_inner:` — the CommandDef double is a non-null object */
    if (def && def->type != JSON_NULL) {
        const char *name = gif_str(json_obj_get(def, "name"));
        if (!name) {
            json_free(def);
            return gif_err("AttributeError", "'SimpleNamespace' object has no attribute 'name'");
        }
        json_t *out = json_array();
        if (strcmp(name, "status") == 0 || strcmp(name, "context") == 0) {
            /* /status and /context are intentionally pre-gate */
            json_t *a = json_object();
            json_set(a, "event", event ? json_copy(event) : json_null());
            json_t *r = gif_call(c, strcmp(name, "status") == 0
                                 ? "self._handle_status_command" : "self._handle_context_command", a);
            if (gif_is_raise(r)) { json_free(out); json_free(def); return r; }
            json_append(out, json_bool(true));
            json_append(out, r);
            json_free(def);
            return out;
        }
        json_t *a2 = json_object();
        json_set(a2, "source", source ? json_copy(source) : json_null());
        json_set_hdr(a2, "command", name);
        json_t *denied = gif_call(c, "self._check_slash_access", a2);
        if (gif_is_raise(denied)) { json_free(out); json_free(def); return denied; }
        if (!gif_is_null(denied)) {  /* _denied is not None ("" included) */
            json_append(out, json_bool(true));
            json_append(out, denied);
            json_free(def);
            return out;
        }
        json_free(denied);
        if (strcmp(name, "stop") == 0 || strcmp(name, "new") == 0) {
            json_t *a3 = json_object();
            json_set(a3, "event", event ? json_copy(event) : json_null());
            json_t *dr = gif_call(c, "self._hm_drop_held_input", a3);
            if (gif_is_raise(dr)) { json_free(out); json_free(def); return dr; }
            json_free(dr);
            json_t *a4 = json_object();
            json_set_hdr(a4, "key", key);
            json_t *so = gif_call(c, "self._hm_stop_owner_turn", a4);
            if (gif_is_raise(so)) { json_free(out); json_free(def); return so; }
            json_free(so);
        }
        json_t *a5 = json_object();
        json_set(a5, "event", event ? json_copy(event) : json_null());
        json_set(a5, "cmd_def", json_copy(def));
        json_set_hdr(a5, "key", key);
        json_set(a5, "source", source ? json_copy(source) : json_null());
        json_t *r = gif_call(c, "self._dispatch_busy_slash_command", a5);
        if (gif_is_raise(r)) { json_free(out); json_free(def); return r; }
        json_append(out, json_bool(true));
        json_append(out, r);
        json_free(def);
        return out;
    }
    json_free(def);
    /* Telegram photo bursts: queue without interrupt */
    if (gif_str_eq(json_obj_get(event, "message_type"), "photo")) {
        json_t *a = json_object();
        json_set(a, "source", source ? json_copy(source) : json_null());
        json_set_hdr(a, "key", key);
        json_set(a, "event", event ? json_copy(event) : json_null());
        json_t *m = gif_call(c, "self._hm_merge_pending_for_source", a);
        GIF_CHK(m);
        json_free(m);
        json_t *out = json_array();
        json_append(out, json_bool(true));
        json_append(out, json_null());
        return out;
    }
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, json_null());
    return out;
}

/* (event, source, is_internal) return tuple as a JSON array. */
static json_t *gif_admit_tuple(json_t *event, json_t *source, bool internal) {
    json_t *out = json_array();
    json_append(out, event ? event : json_null());
    json_append(out, source ? json_copy(source) : json_null());
    json_append(out, json_bool(internal));
    return out;
}

/* PoP: gif_admit_event @ gateway/run_inbound.py:_hm_admit_event */
static json_t *gif_send_unauthorized_decline(gif_ctx_t *c, const json_t *args);
static json_t *gif_admit_event(gif_ctx_t *c, const json_t *args) {
    json_t *event = json_obj_get(args, "event");
    json_t *source = json_obj_get(event, "source");
    json_t *config = json_obj_get(c->state, "config");
    /* try: reset_session_vars() except Exception: logger.debug (silent) */
    json_t *rv = gif_call(c, "reset_session_vars", json_object());
    if (gif_is_exception(rv)) { json_free(rv); rv = NULL; }
    GIF_CHK(rv);
    json_free(rv);
    if (gif_truthy(json_obj_get(config, "multiplex_profiles"))) {
        json_t *a = json_object();
        json_set(a, "source", source ? json_copy(source) : json_null());
        json_t *r = gif_call(c, "self._canonicalize", a);
        GIF_CHK(r);
        json_free(r);
    }
    /* strict `is True`: a truthy non-bool marker does not reject */
    json_t *rejected = json_obj_get(source, "profile_route_rejected");
    if (rejected && rejected->type == JSON_BOOL && rejected->bool_val) {
        json_t *intake = NULL;
        json_t *a = json_object();
        json_set(a, "source", source ? json_copy(source) : json_null());
        json_t *r = gif_call(c, "self._intake_adapter_for", a);
        if (gif_is_exception(r)) { json_free(r); } else { intake = r; }
        json_t *a2 = json_object();
        json_set(a2, "adapter", intake ? intake : json_null());
        json_set(a2, "source", source ? json_copy(source) : json_null());

        json_t *dr = gif_call(c, "drop_rejected_route", a2);
        /* intake was moved into a2 by json_set; gif_call owns a2's nodes. */
        if (gif_is_raise(dr)) return dr;
        json_free(dr);
        return NULL;
    }
    bool is_internal = gif_truthy(json_obj_get(event, "internal"));
    json_t *chat_id = json_obj_get(source, "chat_id");
    bool slack = gif_platform_is(source, "slack");
    json_t *slack_adapter = NULL;
    if (!is_internal && slack) {
        json_t *a = json_object();
        json_set(a, "source", source ? json_copy(source) : json_null());
        json_t *r = gif_call(c, "self._intake_adapter_for", a);
        if (gif_is_exception(r)) { json_free(r); } else { slack_adapter = r; }
    }
    if (!is_internal && slack) {
        json_t *a = json_object();
        json_set(a, "config", config ? json_copy(config) : json_null());
        json_set(a, "chat_id", chat_id ? json_copy(chat_id) : json_null());
        json_set(a, "adapter", slack_adapter ? json_copy(slack_adapter) : json_null());
        json_t *ig = gif_call(c, "_is_slack_ignored_channel", a);
        if (gif_is_raise(ig)) { json_free(slack_adapter); return ig; }
        bool ignored = gif_truthy(ig);
        json_free(ig);
        if (ignored) { json_free(slack_adapter); return NULL; }
    }
    json_free(slack_adapter);
    /* startup-restore queueing: an executing admission is already FIFO-ordered */
    if (gif_truthy(json_obj_get(c->state, "_startup_restore_in_progress"))
        && !is_internal
        && !gif_truthy(json_obj_get(event, "_hermes_startup_restore_replay"))
        && !gif_truthy(json_obj_get(c->state, "executing_admission"))) {
        json_t *a = json_object();
        json_set(a, "event", event ? json_copy(event) : json_null());
        json_t *q = gif_call(c, "self._queue_startup_restore_event", a);
        GIF_CHK(q);
        json_free(q);
        return NULL;
    }
    if (is_internal) return gif_admit_tuple(json_copy(event), source, true);
    if (gif_truthy(json_obj_get(event, "_fifo_only")))
        return gif_admit_tuple(json_copy(event), source, false);
    json_t *sz = gif_call(c, "self._scale_to_zero_note_real_inbound", json_object());
    GIF_CHK(sz);
    json_free(sz);
    json_t *a2 = json_object();
    json_set(a2, "event", event ? json_copy(event) : json_null());
    json_set(a2, "source", source ? json_copy(source) : json_null());
    json_t *hooked = gif_call(c, "self._hm_pre_gateway_dispatch_hook", a2);
    GIF_CHK(hooked);
    if (gif_is_null(hooked)) return NULL;
    event = hooked;  /* owned rewrite; source follows the rewritten event */
    source = json_obj_get(event, "source");
    json_t *a3 = json_object();
    json_set(a3, "source", source ? json_copy(source) : json_null());
    json_t *auth = gif_call(c, "self._is_user_authorized_for_source", a3);
    if (gif_is_raise(auth)) { json_free(event); return auth; }
    bool authorized = gif_truthy(auth);
    json_free(auth);
    if (!authorized) {
        json_t *uid = json_obj_get(source, "user_id");
        if (gif_is_null(uid)) { json_free(event); return NULL; }
        bool pairable = gif_str_eq(json_obj_get(source, "chat_type"), "dm")
            && !gif_truthy(json_obj_get(source, "is_bot"));
        json_t *behavior = NULL;
        if (pairable) {
            json_t *a4 = json_object();
            json_t *plat = json_obj_get(source, "platform");
            json_set(a4, "platform", plat ? json_copy(plat) : json_null());
            json_t *prof = json_obj_get(source, "profile");
            json_set(a4, "profile", prof ? json_copy(prof) : json_null());
            behavior = gif_call(c, "self._get_unauthorized_dm_behavior", a4);
            if (gif_is_raise(behavior)) { json_free(event); return behavior; }
        }
        const char *b = gif_str(behavior);
        json_t *r = NULL;
        if (b && strcmp(b, "pair") == 0) {
            json_t *a5 = json_object();
            json_set(a5, "source", source ? json_copy(source) : json_null());
            r = gif_call(c, "self._hm_offer_pairing_code", a5);
        } else if (b && strcmp(b, "decline") == 0) {
            json_t *a5 = json_object();
            json_set(a5, "source", source ? json_copy(source) : json_null());
            /* self is the real mixin; _hm_send_unauthorized_decline runs its
             * body (pairing-store dedupe) rather than being a script seam. */
            r = gif_send_unauthorized_decline(c, a5);
            json_free(a5);
        } else if (pairable) {
            json_t *a5 = json_object();
            json_set(a5, "source", source ? json_copy(source) : json_null());
            r = gif_call(c, "self._hm_report_ignored_dm", a5);
        }
        json_free(behavior);
        if (r) {
            if (gif_is_raise(r)) { json_free(event); return r; }
            json_free(r);
        }
        json_free(event);
        return NULL;
    }
    /* the busy path charged this event on arrival; a drained follow-up must
     * not pay twice */
    if (!gif_truthy(json_obj_get(event, "_bot_loop_admitted"))) {
        json_t *a6 = json_object();
        json_set(a6, "source", source ? json_copy(source) : json_null());
        json_t *ba = gif_call(c, "self._admit_bot_message_for_source", a6);
        if (gif_is_raise(ba)) { json_free(event); return ba; }
        bool ok = gif_truthy(ba);
        json_free(ba);
        if (ok) {
            json_t *a7 = json_object();
            json_set(a7, "event", json_copy(event));
            json_t *bc = gif_call(c, "self._admit_bot_chain", a7);
            if (gif_is_raise(bc)) { json_free(event); return bc; }
            ok = gif_truthy(bc);
            json_free(bc);
        }
        if (!ok) { json_free(event); return NULL; }
    }
    return gif_admit_tuple(event, source, false);
}

/* PoP: gif_send_unauthorized_decline @ gateway/run_inbound.py:_hm_send_unauthorized_decline */
static json_t *gif_send_unauthorized_decline(gif_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    json_t *plat = json_obj_get(source, "platform");
    const char *platform_name = gif_truthy(plat) ? gif_str(plat) : "unknown";
    if (!platform_name) platform_name = "unknown";
    json_t *a0 = json_object();
    json_set(a0, "source", source ? json_copy(source) : json_null());
    json_t *store = gif_call(c, "self._pairing_store_for", a0);
    GIF_CHK(store);
    if (gif_is_null(store)) return NULL;
    json_free(store);
    json_t *uid = json_obj_get(source, "user_id");
    json_t *a1 = json_object();
    json_set_hdr(a1, "platform", platform_name);
    json_set(a1, "user_id", uid ? json_copy(uid) : json_null());
    json_t *recent = gif_call(c, "pairing_store.has_recent_decline", a1);
    GIF_CHK(recent);
    if (gif_truthy(recent)) { json_free(recent); return NULL; }
    json_free(recent);
    /* the stamp is written BEFORE the send; OSError/TimeoutError -> silent */
    json_t *a2 = json_object();
    json_set_hdr(a2, "platform", platform_name);
    json_set(a2, "user_id", uid ? json_copy(uid) : json_null());
    json_t *rec = gif_call(c, "pairing_store.record_decline", a2);
    if (gif_is_raise(rec)) {
        if (gif_is_oserror(rec)) { json_free(rec); return NULL; }
        return rec;
    }
    json_free(rec);
    json_t *a3 = json_object();
    json_set(a3, "source", source ? json_copy(source) : json_null());
    json_t *adapter = gif_call(c, "self._delivery_adapter_for", a3);
    GIF_CHK(adapter);
    if (!gif_truthy(adapter)) { json_free(adapter); return NULL; }
    json_free(adapter);
    json_t *config = json_obj_get(c->state, "config");
    json_t *msg = json_obj_get(config, "unauthorized_dm_decline_message");
    const char *msg_s = (msg && msg->type == JSON_STRING) ? msg->str_val : "";
    char *text = gif_strip(gif_truthy(msg) ? msg_s : "");
    const char *send_text = (text && text[0]) ? text : GIF_DECLINE_DEFAULT;
    json_t *a4 = json_object();
    json_t *chat = json_obj_get(source, "chat_id");
    json_set(a4, "chat_id", chat ? json_copy(chat) : json_null());
    json_set_hdr(a4, "text", send_text);
    json_t *sr = gif_call(c, "adapter.send", a4);
    free(text);
    if (gif_is_raise(sr)) {
        if (gif_is_exception(sr)) { json_free(sr); return NULL; }
        return sr;
    }
    json_free(sr);
    return NULL;
}
