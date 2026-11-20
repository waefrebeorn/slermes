/*
 * matrix_adapter_flow2.c -- FLOW remainder of
 * plugins/platforms/matrix/adapter.py (hermes-agent): the Matrix gateway
 * adapter's UNCLAIMED authorization / invite-reconcile / DM-cache / reaction
 * / read-receipt / client-op / room-state / typing decision chains. The 36
 * flow1 siblings (matrix_adapter_flow.c) and create_room (matrix.c) are
 * CLAIMED; every collaborator below that is already claimed (or is another
 * ported flow2 target invoked from a sibling flow) enters through the ONE
 * universal seam as "self.<name>" rather than being re-ported.
 *
 * Decision sequences only: every live I/O / injected dependency enters
 * through ma2_call(ctx, name, args). The harness wires it to a recording stub
 * fed the case's scripted results (in.io); the oracle installs recording
 * stubs emitting the identical event shape {"name","args","ret"} from the
 * SAME scripts.
 *
 * Seam name -> live call:
 *   extra_or_secret            -> gateway.platforms._shared.extra_or_secret(extra,key,env,default)
 *   get_scoped_secret          -> gateway.platforms._shared.get_scoped_secret(name,default)
 *   time.time                  -> time.time()
 *   self._is_sender_authorized -> BasePlatformAdapter._is_sender_authorized(user_id,chat_id=)
 *   self._is_authorized_user   -> self._is_authorized_user(user_id,room_id)
 *   self._schedule_invite_join -> self._schedule_invite_join(room_id,is_direct=,inviter=)
 *   self._extract_invite_dm_signal -> self._extract_invite_dm_signal(invited_room)
 *   self._invalidate_room_identities -> self._invalidate_room_identities(room_id)
 *   self._refresh_dm_cache     -> self._refresh_dm_cache()
 *   self._fetch_m_direct       -> self._fetch_m_direct(log_failure=/require_dict=)
 *   self._resolve_room_identity-> self._resolve_room_identity(room_id)
 *   self._state_event_value    -> self._state_event_value(event,key)
 *   client.join_room           -> self._client.join_room(RoomID(room_id))
 *   client.leave_room          -> self._client.leave_room(RoomID(room_id))
 *   client.send_message_event  -> self._client.send_message_event(RoomID,EventType.REACTION,content)
 *   client.set_fully_read_marker -> self._client.set_fully_read_marker(room,event,event)
 *   client.send_receipt        -> self._client.send_receipt(room,event)
 *   client.set_read_markers    -> self._client.set_read_markers(room,fully_read_event=,read_receipt=)
 *   client.get_account_data    -> self._client.get_account_data("m.direct")
 *   client.set_account_data    -> self._client.set_account_data("m.direct",content)
 *   client.redact              -> self._client.redact(RoomID,EventID,reason=)
 *   client.invite_user         -> self._client.invite_user(RoomID,UserID)
 *   client.set_presence        -> self._client.set_presence(presence=,status=)
 *   client.set_typing          -> self._client.set_typing(RoomID,timeout=)
 *   client.get_state_event     -> self._client.get_state_event(RoomID,event_type)
 *   client.joined_members      -> self._client.joined_members(room_id)
 *   state_store.get_members    -> client.state_store.get_members(room_id)
 *   state_store.get_member     -> client.state_store.get_member(room_id,user_id)
 *   coro_factory               -> the _client_op coro_factory() callable
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ma2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_matrix_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Plain JSON numbers are ints; JSON booleans are bools. A Python float in a
 *   type-sensitive slot travels as {"__float__": true, "v": x}; integral
 *   floats normalize to int on both sides (canon).
 * - Attribute doubles (event / client / state_store / resp / member / identity)
 *   are JSON objects. A seam RESULT that live reads as an ATTRIBUTE carrier is
 *   wrapped {"__attr__": {attrs}} (oracle builds SimpleNamespace); a plain
 *   object is a dict. client/state_store METHOD presence is a key carrying
 *   JSON true (oracle binds an async recorder); hasattr == key presence.
 * - set-valued state (_joined_rooms) travels as an array, sorted on emit on
 *   both sides; membership is order-insensitive. _dm_rooms is a dict.
 * - RoomID/EventID/UserID are the import-safe str stubs (identity); EventType
 *   .REACTION == "m.reaction"; PresenceState maps to the bare state string.
 * - logger calls are silent on both sides (not traced); _client_op's
 *   ok_msg/err_msg/level are carried but log-silent (unobservable).
 * - str()/getattr() projections cover the scalar ASCII corpus; .strip()/.lower()
 *   fold ASCII whitespace/case only.
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception / suppress(Exception)
 *   site. The exception-parent table below is the FULL builtin hierarchy.
 * - Fixture domains: client/state_store getters return {null, non-empty
 *   record}; attribute carriers read only present fields; _fetch_m_direct
 *   scripts are dict-or-null; no integral floats in str()-computed positions.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct ma2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ma2_ctx_t;

/* -- exception classification (FULL builtin hierarchy) -- */
static bool ma2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Parent of each builtin exception class; NULL == root (BaseException).
 * Unknown kinds are synthetic Exception subclasses in the oracle (make_exc),
 * so they default to parent "Exception". */
static const char *ma2_exc_parent(const char *k) {
    static const struct { const char *n, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
        {"Exception", "BaseException"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"MemoryError", "Exception"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"SystemError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    if (!k) return "Exception";
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].n, k) == 0) return T[i].p ? T[i].p : "BaseException";
    return "Exception"; /* synthetic kind -> Exception subclass */
}

/* True when the kind is caught by `except Exception` (reaches Exception). */
static bool ma2_exc_is_exception(const char *k) {
    const char *p = k;
    for (int depth = 0; p && depth < 16; depth++) {
        if (strcmp(p, "Exception") == 0) return true;
        if (strcmp(p, "BaseException") == 0) return false;
        p = ma2_exc_parent(p);
    }
    return false;
}

static bool ma2_raise_is_base(const json_t *v) {
    return ma2_is_raise(v) && !ma2_exc_is_exception(json_get_str(v, "__raise__", ""));
}

/* -- truthiness / access -- */
static bool ma2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool ma2_truthy(const json_t *v) {
    if (ma2_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *ma2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ma2_str_eq(const json_t *v, const char *s) {
    const char *t = ma2_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *ma2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static json_t *ma2_make_exc(const json_t *spec) {
    return ma2_err(json_get_str(spec, "type", "RuntimeError"),
                   json_get_str(spec, "message", ""));
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *ma2_call(ma2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ma2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ma2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ma2_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- string helpers (ASCII corpus) -- */
static char *ma2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static char *ma2_strndup(const char *s, size_t n) {
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static bool ma2_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *ma2_strip(const char *s) {
    if (!s) s = "";
    while (*s && ma2_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && ma2_is_ascii_ws((unsigned char)end[-1])) end--;
    return ma2_strndup(s, (size_t)(end - s));
}

static char *ma2_lower(const char *s) {
    char *o = ma2_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static bool ma2_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static const char *ma2_type_name(const json_t *v) {
    if (ma2_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* str() projection of a scalar (fixtures avoid containers in str() slots). */
static char *ma2_py_str(const json_t *v) {
    if (ma2_is_null(v)) return ma2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ma2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return ma2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return ma2_strdup(buf);
        }
        default: return ma2_strdup("");
    }
}

/* getattr(obj, key, default) then str(): absent -> default; else str(value). */
static char *ma2_getattr_str(const json_t *obj, const char *key, const char *def) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) return ma2_strdup(def);
    return ma2_py_str(v);
}

/* -- attribute-double access ({"__attr__": {attrs}} wrappers) -- */
static bool ma2_is_ad(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_has(v, "__attr__");
}
static const json_t *ma2_ad_inner(const json_t *v) {
    return ma2_is_ad(v) ? json_obj_get(v, "__attr__") : NULL;
}
static const json_t *ma2_ad_attr(const json_t *v, const char *key) {
    const json_t *in = ma2_ad_inner(v);
    return in ? json_obj_get(in, key) : NULL;
}
static bool ma2_ad_has(const json_t *v, const char *key) {
    const json_t *in = ma2_ad_inner(v);
    return in && json_has(in, key);
}

/* -- self / state access -- */
static const json_t *ma2_self_attr(const json_t *args, const char *name) {
    const json_t *slf = json_obj_get(args, "self");
    return slf ? json_obj_get(slf, name) : NULL;
}
static const json_t *ma2_client(const json_t *args) {
    return ma2_self_attr(args, "_client");
}
static json_t *ma2_state_get(ma2_ctx_t *ctx, const char *key) {
    return json_obj_get(ctx->state, key);
}

/* -- numeric helpers (int / __float__ tag) -- */
static bool ma2_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool ma2_is_number(const json_t *v) {
    return (v && v->type == JSON_NUMBER) || ma2_is_float_tag(v);
}
static double ma2_num(const json_t *v) {
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (ma2_is_float_tag(v)) return json_get_num(v, "v", 0);
    return 0.0;
}
static const char *ma2_num_type(const json_t *v) {
    if (ma2_is_float_tag(v)) return "float";
    return ma2_type_name(v);
}

/* a - b with the operand-type TypeError live raises on a non-number. */
static json_t *ma2_sub(const json_t *a, const json_t *b) {
    if (!ma2_is_number(a) || !ma2_is_number(b)) {
        char msg[256];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for -: '%s' and '%s'",
                 ma2_num_type(a), ma2_num_type(b));
        return ma2_err("TypeError", msg);
    }
    return json_number(ma2_num(a) - ma2_num(b));
}

/* len(v): array/object/string -> count; else the live TypeError. */
static json_t *ma2_len(const json_t *v) {
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT))
        return json_number((double)v->c.count);
    if (v && v->type == JSON_STRING)
        return json_number((double)(v->str_val ? strlen(v->str_val) : 0));
    char msg[128];
    snprintf(msg, sizeof msg, "object of type '%s' has no len()", ma2_type_name(v));
    return ma2_err("TypeError", msg);
}

/* -- string vector / set helpers -- */
typedef struct { char **v; size_t n, cap; } ma2_strvec_t;
static void ma2_sv_push(ma2_strvec_t *sv, char *s) { /* adopts s */
    if (sv->n == sv->cap) {
        sv->cap = sv->cap ? sv->cap * 2 : 8;
        sv->v = realloc(sv->v, sv->cap * sizeof(char *));
    }
    sv->v[sv->n++] = s;
}
static int ma2_sv_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static json_t *ma2_sv_to_sorted_set(ma2_strvec_t *sv) {
    if (sv->n > 1) qsort(sv->v, sv->n, sizeof(char *), ma2_sv_cmp);
    json_t *arr = json_array();
    for (size_t i = 0; i < sv->n; i++) {
        if (i == 0 || strcmp(sv->v[i], sv->v[i - 1]) != 0)
            json_append(arr, json_string(sv->v[i]));
        free(sv->v[i]);
    }
    free(sv->v);
    return arr;
}
static bool ma2_arr_contains(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (ma2_str_eq(json_get(arr, i), s)) return true;
    return false;
}
static void ma2_set_add(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return;
    if (!ma2_arr_contains(arr, s)) json_append(arr, json_string(s));
}
/* sorted copy of a string array (for set-valued state echo). */
static json_t *ma2_sorted_copy(const json_t *arr) {
    ma2_strvec_t sv = {NULL, 0, 0};
    if (arr && arr->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(arr); i++) {
            const char *s = ma2_str(json_get(arr, i));
            ma2_sv_push(&sv, ma2_strdup(s ? s : ""));
        }
    return ma2_sv_to_sorted_set(&sv);
}
/* ensure a state key holds an array (set model); returns the owned array. */
static json_t *ma2_state_array(ma2_ctx_t *ctx, const char *key) {
    json_t *a = json_obj_get(ctx->state, key);
    if (!a || a->type != JSON_ARRAY) {
        a = json_array();
        json_set(ctx->state, key, a);
    }
    return a;
}
/* == config static helpers (extra_or_secret seam) == */

/* shared core: configured if bool else str(configured).lower() membership.
 * mode 0: in ("true","1","yes"); mode 1: not in ("false","0","no","off"). */
static json_t *ma2_truthy_configured(ma2_ctx_t *ctx, const json_t *extra,
                                     const char *key, const char *env,
                                     const json_t *def, int mode) {
    json_t *a = json_object();
    json_set(a, "extra", extra ? json_copy(extra) : json_object());
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "env", env);
    json_set(a, "default", def ? json_copy(def) : json_null());
    json_t *c = ma2_call(ctx, "extra_or_secret", a);
    if (ma2_is_raise(c)) return c; /* no try/except: propagate */
    if (c->type == JSON_BOOL) return c;
    char *s = ma2_py_str(c);
    char *low = ma2_lower(s);
    free(s);
    bool hit;
    if (mode == 0)
        hit = strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
    else
        hit = !(strcmp(low, "false") == 0 || strcmp(low, "0") == 0
             || strcmp(low, "no") == 0 || strcmp(low, "off") == 0);
    free(low);
    json_free(c);
    return json_bool(hit);
}

/* PoP: ma2_extra_truthy @ plugins/platforms/matrix/adapter.py:_extra_truthy */
static json_t *ma2_extra_truthy(ma2_ctx_t *ctx, const json_t *args) {
    return ma2_truthy_configured(ctx, json_obj_get(args, "extra"),
        json_get_str(args, "key", ""), json_get_str(args, "env_name", ""),
        json_obj_get(args, "default"), 0);
}

/* PoP: ma2_parse_require_mention @ plugins/platforms/matrix/adapter.py:_parse_require_mention */
static json_t *ma2_parse_require_mention(ma2_ctx_t *ctx, const json_t *args) {
    json_t *def = json_bool(true);
    json_t *r = ma2_truthy_configured(ctx, json_obj_get(args, "extra"),
        "require_mention", "MATRIX_REQUIRE_MENTION", def, 1);
    json_free(def);
    return r;
}

/* PoP: ma2_parse_thread_require_mention @ plugins/platforms/matrix/adapter.py:_parse_thread_require_mention */
static json_t *ma2_parse_thread_require_mention(ma2_ctx_t *ctx, const json_t *args) {
    json_t *def = json_bool(false);
    json_t *r = ma2_truthy_configured(ctx, json_obj_get(args, "extra"),
        "thread_require_mention", "MATRIX_THREAD_REQUIRE_MENTION", def, 1);
    json_free(def);
    return r;
}

/* == clock-skew detector (stateful, time.time seam) == */

/* PoP: ma2_reset_clock_skew_detector @ plugins/platforms/matrix/adapter.py:_reset_clock_skew_detector */
static json_t *ma2_reset_clock_skew_detector(ma2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_set(ctx->state, "_late_grace_drops", json_number(0));
    json_set(ctx->state, "_late_grace_skew", json_number(0.0));
    json_set(ctx->state, "_clock_skew_warned", json_bool(false));
    return json_null();
}

/* PoP: ma2_note_late_grace_drop @ plugins/platforms/matrix/adapter.py:_note_late_grace_drop */
static json_t *ma2_note_late_grace_drop(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *warned = ma2_state_get(ctx, "_clock_skew_warned");
    if (ma2_truthy(warned)) return json_null(); /* short-circuit: no time.time */
    json_t *tt = ma2_call(ctx, "time.time", json_object());
    if (ma2_is_raise(tt)) return tt; /* no try/except */
    const json_t *startup = ma2_self_attr(args, "_startup_ts");
    json_t *diff = ma2_sub(tt, startup);
    json_free(tt);
    if (ma2_is_raise(diff)) return diff;
    bool early = diff->num_val <= 30.0;
    json_free(diff);
    if (early) return json_null();
    const json_t *event_ts = json_obj_get(args, "event_ts");
    json_t *skewj = ma2_sub(startup, event_ts);
    if (ma2_is_raise(skewj)) return skewj;
    double skew = skewj->num_val;
    json_free(skewj);
    if (!(5.0 < skew && skew < 86400.0)) return json_null();
    double drops = json_get_num(ctx->state, "_late_grace_drops", 0);
    double lgskew = json_get_num(ctx->state, "_late_grace_skew", 0);
    if (drops != 0.0 && fabs(skew - lgskew) < 60.0) {
        drops += 1.0;
    } else {
        lgskew = skew;
        drops = 1.0;
    }
    json_set(ctx->state, "_late_grace_skew", json_number(lgskew));
    json_set(ctx->state, "_late_grace_drops", json_number(drops));
    if (drops >= 3.0)
        json_set(ctx->state, "_clock_skew_warned", json_bool(true)); /* logger silent */
    return json_null();
}

/* == authorization == */

/* PoP: ma2_is_authorized_user @ plugins/platforms/matrix/adapter.py:_is_authorized_user */
static json_t *ma2_is_authorized_user(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *user_id = json_obj_get(args, "user_id");
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *ac = ma2_self_attr(args, "_authorization_check");
    if (ac && !ma2_is_null(ac)) { /* getattr(...) is not None */
        json_t *a = json_object();
        json_set(a, "user_id", user_id ? json_copy(user_id) : json_null());
        json_set(a, "chat_id", room_id ? json_copy(room_id) : json_null());
        json_t *r = ma2_call(ctx, "self._is_sender_authorized", a);
        if (ma2_is_raise(r)) return r;
        bool is_true = r->type == JSON_BOOL && r->bool_val; /* `is True` identity */
        json_free(r);
        return json_bool(is_true);
    }
    json_t *a = json_object();
    json_set_hdr(a, "name", "GATEWAY_ALLOW_ALL_USERS");
    json_set_hdr(a, "default", "");
    json_t *sc = ma2_call(ctx, "get_scoped_secret", a);
    if (ma2_is_raise(sc)) return sc;
    if (sc->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ma2_type_name(sc));
        json_free(sc);
        return ma2_err("AttributeError", msg);
    }
    char *t = ma2_strip(sc->str_val);
    char *low = ma2_lower(t);
    free(t);
    bool allow = strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
    free(low);
    json_free(sc);
    if (allow) return json_bool(true);
    const json_t *auids = ma2_self_attr(args, "_allowed_user_ids");
    const char *uid = ma2_str(user_id);
    bool member = ma2_truthy(auids) && uid && ma2_arr_contains(auids, uid);
    return json_bool(member);
}

/* == invite flows == */

/* PoP: ma2_on_invite @ plugins/platforms/matrix/adapter.py:_on_invite */
static json_t *ma2_on_invite(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *event = json_obj_get(args, "event");
    char *room_id = ma2_getattr_str(event, "room_id", "");
    const json_t *content = event ? json_obj_get(event, "content") : NULL;
    const json_t *isd = (content && content->type == JSON_OBJECT)
        ? json_obj_get(content, "is_direct") : NULL;
    bool is_direct = ma2_truthy(isd);
    char *inviter = ma2_getattr_str(event, "sender", "");
    json_t *a = json_object();
    json_set_hdr(a, "user_id", inviter);
    json_set_hdr(a, "room_id", room_id);
    json_t *auth = ma2_call(ctx, "self._is_authorized_user", a);
    if (ma2_is_raise(auth)) { free(room_id); free(inviter); return auth; }
    bool authorized = ma2_truthy(auth);
    json_free(auth);
    if (!authorized) { free(room_id); free(inviter); return json_null(); }
    json_t *s = json_object();
    json_set_hdr(s, "room_id", room_id);
    json_set(s, "is_direct", json_bool(is_direct && inviter[0] != '\0'));
    json_set_hdr(s, "inviter", inviter);
    json_t *r = ma2_call(ctx, "self._schedule_invite_join", s);
    free(room_id); free(inviter);
    if (ma2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ma2_schedule_pending_invite_joins @ plugins/platforms/matrix/adapter.py:_schedule_pending_invite_joins */
static json_t *ma2_schedule_pending_invite_joins(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *sync_data = json_obj_get(args, "sync_data");
    const json_t *invites = NULL;
    json_t *empty = NULL;
    if (sync_data && sync_data->type == JSON_OBJECT) {
        const json_t *rooms = json_obj_get(sync_data, "rooms");
        if (!rooms) { empty = json_object(); rooms = empty; } /* .get("rooms", {}) */
        if (ma2_is_null(rooms) || rooms->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", ma2_type_name(rooms));
            json_free(empty);
            return ma2_err("AttributeError", msg);
        }
        invites = json_obj_get(rooms, "invite");
        if (!invites) { json_free(empty); empty = json_object(); invites = empty; }
    } else {
        empty = json_object(); invites = empty; /* else {} */
    }
    if (invites->type != JSON_OBJECT) { json_free(empty); return json_null(); }
    json_t *joined = ma2_state_array(ctx, "_joined_rooms");
    json_t *ret = json_null();
    for (size_t i = 0; i < json_object_size(invites); i++) {
        const char *room_id = json_object_get_key_at(invites, i);
        const json_t *invited_room = json_object_get_at(invites, i);
        if (ma2_arr_contains(joined, room_id)) continue;
        json_t *a = json_object();
        json_set(a, "invited_room", invited_room ? json_copy(invited_room) : json_null());
        json_t *sig = ma2_call(ctx, "self._extract_invite_dm_signal", a);
        if (ma2_is_raise(sig)) { ret = sig; break; }
        const json_t *isd = json_get(sig, 0);
        const json_t *inv = json_get(sig, 1);
        char *inviter = ma2_py_str(inv);
        json_t *au = json_object();
        json_set_hdr(au, "user_id", inviter);
        json_set_hdr(au, "room_id", room_id);
        json_t *auth = ma2_call(ctx, "self._is_authorized_user", au);
        if (ma2_is_raise(auth)) { free(inviter); json_free(sig); ret = auth; break; }
        bool authorized = ma2_truthy(auth);
        json_free(auth);
        if (!authorized) { free(inviter); json_free(sig); continue; }
        json_t *s = json_object();
        json_set_hdr(s, "room_id", room_id);
        json_set(s, "is_direct", json_bool(ma2_truthy(isd)));
        json_set_hdr(s, "inviter", inviter);
        json_t *r = ma2_call(ctx, "self._schedule_invite_join", s);
        free(inviter); json_free(sig);
        if (ma2_is_raise(r)) { ret = r; break; }
        json_free(r);
    }
    json_free(empty);
    return ret;
}

/* PoP: ma2_extract_invite_dm_signal @ plugins/platforms/matrix/adapter.py:_extract_invite_dm_signal */
static json_t *ma2_extract_invite_dm_signal(ma2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, json_string(""));
    const json_t *uid = ma2_self_attr(args, "_user_id");
    if (!ma2_truthy(uid)) return out;
    const json_t *invited_room = json_obj_get(args, "invited_room");
    if (!invited_room || invited_room->type != JSON_OBJECT) return out;
    const json_t *invite_state = json_obj_get(invited_room, "invite_state");
    if (!invite_state || invite_state->type != JSON_OBJECT) return out;
    const json_t *events = json_obj_get(invite_state, "events");
    if (!events || events->type != JSON_ARRAY) return out;
    const char *user_id = ma2_str(uid);
    for (size_t i = 0; i < json_len(events); i++) {
        const json_t *ev = json_get(events, i);
        if (!ev || ev->type != JSON_OBJECT) continue;
        if (!ma2_str_eq(json_obj_get(ev, "type"), "m.room.member")) continue;
        if (!ma2_str_eq(json_obj_get(ev, "state_key"), user_id)) continue;
        const json_t *content = json_obj_get(ev, "content");
        if (!content || content->type != JSON_OBJECT) continue;
        if (!ma2_str_eq(json_obj_get(content, "membership"), "invite")) continue;
        bool is_direct = ma2_truthy(json_obj_get(content, "is_direct"));
        const json_t *sender = json_obj_get(ev, "sender");
        char *s = sender ? ma2_py_str(sender) : ma2_strdup(""); /* .get("sender","") */
        json_free(out);
        out = json_array();
        json_append(out, json_bool(is_direct));
        json_append(out, json_string(s));
        free(s);
        return out;
    }
    return out;
}

/* PoP: ma2_join_room_by_id @ plugins/platforms/matrix/adapter.py:_join_room_by_id */
static json_t *ma2_join_room_by_id(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *room_id = json_obj_get(args, "room_id");
    const char *rid = ma2_str(room_id);
    json_t *joined = ma2_state_array(ctx, "_joined_rooms");
    if (!ma2_truthy(room_id) || (rid && ma2_arr_contains(joined, rid)))
        return json_bool(ma2_truthy(room_id));
    const json_t *client = ma2_client(args);
    json_t *exc = NULL; /* the caught Exception descriptor (NULL on success) */
    if (ma2_is_null(client)) {
        exc = ma2_err("AttributeError", "'NoneType' object has no attribute 'join_room'");
    } else {
        json_t *a = json_object();
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_t *r = ma2_call(ctx, "client.join_room", a);
        if (ma2_is_raise(r)) {
            if (ma2_raise_is_base(r)) return r; /* passthrough */
            exc = r;
        } else {
            json_free(r);
        }
    }
    if (!exc) { /* success path (inside try) */
        if (rid) ma2_set_add(joined, rid);
        json_t *a = json_object();
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_t *inv = ma2_call(ctx, "self._invalidate_room_identities", a);
        if (ma2_is_raise(inv)) {
            if (ma2_raise_is_base(inv)) return inv;
            exc = inv;
        } else {
            json_free(inv);
        }
    }
    if (!exc) {
        json_t *rf = ma2_call(ctx, "self._refresh_dm_cache", json_object());
        if (ma2_is_raise(rf)) {
            if (ma2_raise_is_base(rf)) return rf;
            exc = rf;
        } else {
            json_free(rf);
        }
    }
    if (!exc) return json_bool(true);
    /* except Exception: msg = str(exc).lower(); dead-invite leave */
    char *msg = ma2_lower(json_get_str(exc, "message", ""));
    bool dead = strstr(msg, "no servers") != NULL || strstr(msg, "room not found") != NULL;
    free(msg);
    json_free(exc);
    if (dead) {
        /* with suppress(Exception): await client.leave_room(...) */
        if (!ma2_is_null(client)) {
            json_t *a = json_object();
            json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
            json_t *lr = ma2_call(ctx, "client.leave_room", a);
            if (ma2_is_raise(lr)) {
                if (ma2_raise_is_base(lr)) return lr; /* suppress passes base */
                json_free(lr);
            } else {
                json_free(lr);
            }
        }
    }
    return json_bool(false);
}
/* == DM cache flows == */

/* PoP: ma2_fetch_m_direct @ plugins/platforms/matrix/adapter.py:_fetch_m_direct */
static json_t *ma2_fetch_m_direct(ma2_ctx_t *ctx, const json_t *args) {
    bool require_dict = ma2_truthy(json_obj_get(args, "require_dict"));
    const json_t *client = ma2_client(args);
    if (ma2_is_null(client)) {
        /* AttributeError caught by except Exception -> None (log silent) */
        return json_null();
    }
    json_t *a = json_object();
    json_set_hdr(a, "key", "m.direct");
    json_t *resp = ma2_call(ctx, "client.get_account_data", a);
    if (ma2_is_raise(resp)) {
        if (ma2_raise_is_base(resp)) return resp;
        json_free(resp);
        return json_null(); /* except Exception -> None */
    }
    json_t *ret = json_null();
    if (ma2_is_ad(resp)) {
        /* hasattr(resp,"content") and (not require_dict or isinstance(content,dict)) */
        if (ma2_ad_has(resp, "content")) {
            const json_t *content = ma2_ad_attr(resp, "content");
            if (!require_dict || (content && content->type == JSON_OBJECT))
                ret = content ? json_copy(content) : json_null();
        }
        /* else: resp is a SimpleNamespace (not a dict) -> None */
    } else if (resp && resp->type == JSON_OBJECT) {
        ret = json_copy(resp); /* plain dict resp */
    }
    json_free(resp);
    return ret;
}

/* PoP: ma2_refresh_dm_cache @ plugins/platforms/matrix/adapter.py:_refresh_dm_cache */
static json_t *ma2_refresh_dm_cache(ma2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client)) return json_null();
    json_t *a = json_object();
    json_set(a, "log_failure", json_bool(true));
    json_t *dm = ma2_call(ctx, "self._fetch_m_direct", a);
    if (ma2_is_raise(dm)) return dm; /* not in a try */
    if (ma2_is_null(dm)) { json_free(dm); return json_null(); }
    if (dm->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'values'", ma2_type_name(dm));
        json_free(dm);
        return ma2_err("AttributeError", msg);
    }
    ma2_strvec_t ids = {NULL, 0, 0};
    for (size_t i = 0; i < json_object_size(dm); i++) {
        const json_t *rooms = json_object_get_at(dm, i);
        if (!rooms || rooms->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(rooms); j++) {
            const json_t *r = json_get(rooms, j);
            if (r && r->type == JSON_STRING)
                ma2_sv_push(&ids, ma2_strdup(r->str_val ? r->str_val : ""));
        }
    }
    json_t *idset = ma2_sv_to_sorted_set(&ids);
    json_t *joined = ma2_state_array(ctx, "_joined_rooms");
    json_t *dm_rooms = json_object();
    for (size_t i = 0; i < json_len(joined); i++) {
        const char *rid = ma2_str(json_get(joined, i));
        if (!rid) continue;
        json_set(dm_rooms, rid, json_bool(ma2_arr_contains(idset, rid)));
    }
    json_set(ctx->state, "_dm_rooms", dm_rooms);
    json_free(idset);
    json_free(dm);
    json_t *inv = ma2_call(ctx, "self._invalidate_room_identities", json_object());
    /* room_id defaults None -> args {room_id: null} */
    if (ma2_is_raise(inv)) return inv;
    json_free(inv);
    return json_null();
}

/* PoP: ma2_record_dm_room @ plugins/platforms/matrix/adapter.py:_record_dm_room */
static json_t *ma2_record_dm_room(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client)) return json_null();
    const char *room_id = json_get_str(args, "room_id", "");
    const char *inviter = json_get_str(args, "inviter", "");
    json_t *a = json_object();
    json_set(a, "require_dict", json_bool(true));
    json_t *dm = ma2_call(ctx, "self._fetch_m_direct", a);
    if (ma2_is_raise(dm)) return dm;
    json_t *dm_data;
    if (dm && dm->type == JSON_OBJECT && ma2_truthy(dm)) {
        dm_data = dm; /* take ownership of the seam result */
    } else {
        json_free(dm);
        dm_data = json_object(); /* `or {}` */
    }
    const json_t *rooms_node = json_obj_get(dm_data, inviter);
    bool borrowed = rooms_node && rooms_node->type == JSON_ARRAY;
    json_t *rooms = borrowed ? (json_t *)rooms_node : json_array();
    bool changed = false;
    if (!ma2_arr_contains(rooms, room_id)) {
        json_append(rooms, json_string(room_id));
        if (!borrowed) json_set(dm_data, inviter, rooms); /* fresh; old non-list freed */
        changed = true;
    }
    if (changed && !ma2_is_null(client)) {
        json_t *sa_args = json_object();
        json_set_hdr(sa_args, "key", "m.direct");
        json_set(sa_args, "content", json_copy(dm_data));
        json_t *sa = ma2_call(ctx, "client.set_account_data", sa_args);
        if (ma2_is_raise(sa)) {
            if (ma2_raise_is_base(sa)) { json_free(dm_data); return sa; }
            json_free(sa); /* except Exception: log silent */
        } else {
            json_free(sa);
        }
    }
    json_free(dm_data);
    json_t *dm_rooms = json_obj_get(ctx->state, "_dm_rooms");
    if (!dm_rooms || dm_rooms->type != JSON_OBJECT) {
        dm_rooms = json_object();
        json_set(ctx->state, "_dm_rooms", dm_rooms);
    }
    json_set(dm_rooms, room_id, json_bool(true));
    json_t *ia = json_object();
    json_set_hdr(ia, "room_id", room_id);
    json_t *inv = ma2_call(ctx, "self._invalidate_room_identities", ia);
    if (ma2_is_raise(inv)) return inv;
    json_free(inv);
    return json_null();
}

/* PoP: ma2_is_dm_room @ plugins/platforms/matrix/adapter.py:_is_dm_room */
static json_t *ma2_is_dm_room(ma2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(json_obj_get(args, "room_id")));
    json_t *r = ma2_call(ctx, "self._resolve_room_identity", a);
    if (ma2_is_raise(r)) return r;
    json_t *ret;
    if (ma2_is_ad(r)) {
        const json_t *ct = ma2_ad_attr(r, "chat_type");
        if (!ct) {
            ret = ma2_err("AttributeError",
                "'types.SimpleNamespace' object has no attribute 'chat_type'");
        } else {
            ret = json_bool(ma2_str_eq(ct, "dm"));
        }
    } else {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'chat_type'", ma2_type_name(r));
        ret = ma2_err("AttributeError", msg);
    }
    json_free(r);
    return ret;
}

/* == reactions / receipts == */

/* PoP: ma2_send_reaction @ plugins/platforms/matrix/adapter.py:_send_reaction */
static json_t *ma2_send_reaction(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client)) return json_null();
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *event_id = json_obj_get(args, "event_id");
    const json_t *emoji = json_obj_get(args, "emoji");
    json_t *rel = json_object();
    json_set_hdr(rel, "rel_type", "m.annotation");
    json_set(rel, "event_id", event_id ? json_copy(event_id) : json_null());
    json_set(rel, "key", emoji ? json_copy(emoji) : json_null());
    json_t *content = json_object();
    json_set(content, "m.relates_to", rel);
    json_t *a = json_object();
    json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
    json_set_hdr(a, "event_type", "m.reaction");
    json_set(a, "content", content);
    json_t *r = ma2_call(ctx, "client.send_message_event", a);
    if (ma2_is_raise(r)) {
        if (ma2_raise_is_base(r)) return r;
        json_free(r);
        return json_null(); /* except Exception -> None */
    }
    char *s = ma2_py_str(r);
    json_t *ret = json_string(s);
    free(s);
    json_free(r);
    return ret;
}

/* PoP: ma2_send_read_receipt @ plugins/platforms/matrix/adapter.py:send_read_receipt */
static json_t *ma2_send_read_receipt(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client)) return json_bool(false);
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *event_id = json_obj_get(args, "event_id");
    json_t *a = json_object();
    const char *seam = NULL;
    if (json_has(client, "set_fully_read_marker")) {
        seam = "client.set_fully_read_marker";
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_set(a, "fully_read_event", event_id ? json_copy(event_id) : json_null());
        json_set(a, "read_receipt", event_id ? json_copy(event_id) : json_null());
    } else if (json_has(client, "send_receipt")) {
        seam = "client.send_receipt";
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_set(a, "event_id", event_id ? json_copy(event_id) : json_null());
    } else if (json_has(client, "set_read_markers")) {
        seam = "client.set_read_markers";
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_set(a, "fully_read_event", event_id ? json_copy(event_id) : json_null());
        json_set(a, "read_receipt", event_id ? json_copy(event_id) : json_null());
    } else {
        json_free(a);
        return json_bool(false); /* no read receipt method (log silent) */
    }
    json_t *r = ma2_call(ctx, seam, a);
    if (ma2_is_raise(r)) {
        if (ma2_raise_is_base(r)) return r;
        json_free(r);
        return json_bool(false); /* except Exception -> False */
    }
    json_free(r);
    return json_bool(true);
}

/* == client-op core (logger silent: ok_msg/err_msg/level unobservable) == */

/* _client_op decision: client present -> run coro_factory -> True; an
 * Exception is caught -> False; BaseException passes through. */
static json_t *ma2_client_op_core(ma2_ctx_t *ctx, const json_t *client,
                                  const char *seam, json_t *op_args) {
    if (!ma2_truthy(client)) { json_free(op_args); return json_bool(false); }
    json_t *r = ma2_call(ctx, seam, op_args);
    if (ma2_is_raise(r)) {
        if (ma2_raise_is_base(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}

/* PoP: ma2_client_op @ plugins/platforms/matrix/adapter.py:_client_op */
static json_t *ma2_client_op(ma2_ctx_t *ctx, const json_t *args) {
    (void)args; /* ok_msg/err_msg/level carried but log-silent */
    return ma2_client_op_core(ctx, ma2_client(args), "coro_factory", json_object());
}

/* PoP: ma2_redact_message @ plugins/platforms/matrix/adapter.py:redact_message */
static json_t *ma2_redact_message(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *reason = json_obj_get(args, "reason");
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(json_obj_get(args, "room_id")));
    json_set(a, "event_id", json_copy(json_obj_get(args, "event_id")));
    json_set(a, "reason", ma2_truthy(reason) ? json_copy(reason) : json_null());
    return ma2_client_op_core(ctx, ma2_client(args), "client.redact", a);
}

/* PoP: ma2_invite_user @ plugins/platforms/matrix/adapter.py:invite_user */
static json_t *ma2_invite_user(ma2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(json_obj_get(args, "room_id")));
    json_set(a, "user_id", json_copy(json_obj_get(args, "user_id")));
    return ma2_client_op_core(ctx, ma2_client(args), "client.invite_user", a);
}

/* PoP: ma2_set_presence @ plugins/platforms/matrix/adapter.py:set_presence */
static json_t *ma2_set_presence(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client)) return json_bool(false);
    const json_t *state = json_obj_get(args, "state");
    /* `state not in _VALID_PRESENCE_STATES` (frozenset): unhashable raises */
    if (state && (state->type == JSON_ARRAY || state->type == JSON_OBJECT)) {
        char msg[160];
        snprintf(msg, sizeof msg, "cannot use '%s' as a set element (unhashable type: '%s')",
                 ma2_type_name(state), ma2_type_name(state));
        return ma2_err("TypeError", msg);
    }
    const char *s = ma2_str(state);
    bool valid = s && (strcmp(s, "online") == 0 || strcmp(s, "offline") == 0
                    || strcmp(s, "unavailable") == 0);
    if (!valid) return json_bool(false); /* invalid state (log silent) */
    const json_t *status_msg = json_obj_get(args, "status_msg");
    json_t *a = json_object();
    json_set_hdr(a, "presence", s); /* PresenceState stub maps to the bare string */
    json_set(a, "status", ma2_truthy(status_msg) ? json_copy(status_msg) : json_null());
    return ma2_client_op_core(ctx, client, "client.set_presence", a);
}

/* == room state / display name == */

/* PoP: ma2_get_room_member_count @ plugins/platforms/matrix/adapter.py:_get_room_member_count */
static json_t *ma2_get_room_member_count(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *state_store = ma2_truthy(client) ? json_obj_get(client, "state_store") : NULL;
    if (ma2_truthy(state_store)) {
        /* with suppress(Exception): members = await state_store.get_members */
        json_t *a = json_object();
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_t *m = ma2_call(ctx, "state_store.get_members", a);
        if (ma2_is_raise(m)) {
            if (ma2_raise_is_base(m)) return m;
            json_free(m); /* suppressed -> fall through */
        } else if (!ma2_is_null(m)) {
            json_t *l = ma2_len(m);
            json_free(m);
            if (ma2_is_raise(l)) {
                if (ma2_raise_is_base(l)) return l;
                json_free(l); /* len TypeError suppressed -> fall through */
            } else {
                return l;
            }
        } else {
            json_free(m);
        }
    }
    /* client = getattr(self,"_client",None); hasattr(client,"joined_members") */
    if (!ma2_is_null(client) && json_has(client, "joined_members")) {
        json_t *a = json_object();
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_t *resp = ma2_call(ctx, "client.joined_members", a);
        if (ma2_is_raise(resp)) {
            if (ma2_raise_is_base(resp)) return resp;
            json_free(resp);
            return json_null(); /* suppressed */
        }
        const json_t *members = ma2_is_ad(resp) ? ma2_ad_attr(resp, "members") : NULL;
        if (!ma2_is_null(members)) {
            json_t *l = ma2_len(members);
            if (ma2_is_raise(l)) {
                if (ma2_raise_is_base(l)) { json_free(resp); return l; }
                json_free(l);
                json_free(resp);
                return json_null();
            }
            json_free(resp);
            return l;
        }
        json_free(resp);
    }
    return json_null();
}

/* PoP: ma2_get_room_state_value @ plugins/platforms/matrix/adapter.py:_get_room_state_value */
static json_t *ma2_get_room_state_value(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    if (!ma2_truthy(client) || !json_has(client, "get_state_event")) return json_null();
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *event_type = json_obj_get(args, "event_type");
    const json_t *key = json_obj_get(args, "key");
    json_t *a = json_object();
    json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
    json_set(a, "event_type", event_type ? json_copy(event_type) : json_null());
    json_t *event = ma2_call(ctx, "client.get_state_event", a);
    if (ma2_is_raise(event)) {
        if (ma2_raise_is_base(event)) return event;
        json_free(event);
        return json_null(); /* except Exception -> None */
    }
    /* _state_event_value call is OUTSIDE the try -> its raise propagates */
    json_t *sa = json_object();
    json_set(sa, "event", json_copy(event));
    json_set(sa, "key", key ? json_copy(key) : json_null());
    json_free(event);
    json_t *sv = ma2_call(ctx, "self._state_event_value", sa);
    if (ma2_is_raise(sv)) return sv;
    /* value = (sv or "").strip(); return value or None */
    const json_t *val = ma2_truthy(sv) ? sv : NULL;
    if (val && val->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ma2_type_name(val));
        json_free(sv);
        return ma2_err("AttributeError", msg);
    }
    char *stripped = ma2_strip(val ? val->str_val : "");
    json_t *ret = (stripped && stripped[0]) ? json_string(stripped) : json_null();
    free(stripped);
    json_free(sv);
    return ret;
}

/* PoP: ma2_get_display_name @ plugins/platforms/matrix/adapter.py:_get_display_name */
static json_t *ma2_get_display_name(ma2_ctx_t *ctx, const json_t *args) {
    const json_t *client = ma2_client(args);
    const json_t *room_id = json_obj_get(args, "room_id");
    const json_t *user_id = json_obj_get(args, "user_id");
    const json_t *state_store = ma2_truthy(client) ? json_obj_get(client, "state_store") : NULL;
    if (ma2_truthy(state_store)) {
        json_t *a = json_object();
        json_set(a, "room_id", room_id ? json_copy(room_id) : json_null());
        json_set(a, "user_id", user_id ? json_copy(user_id) : json_null());
        json_t *m = ma2_call(ctx, "state_store.get_member", a);
        if (ma2_is_raise(m)) {
            if (ma2_raise_is_base(m)) return m;
            json_free(m); /* suppressed -> fallback */
        } else {
            if (ma2_truthy(m) && ma2_is_ad(m)) {
                const json_t *dn = ma2_ad_attr(m, "displayname");
                if (ma2_truthy(dn)) {
                    json_t *ret = json_copy(dn);
                    json_free(m);
                    return ret;
                }
            }
            json_free(m);
        }
    }
    const char *uid = ma2_str(user_id);
    if (!uid) uid = "";
    if (ma2_starts_with(uid, "@") && strchr(uid, ':')) {
        const char *p = strchr(uid + 1, ':');
        char *part = p ? ma2_strndup(uid + 1, (size_t)(p - (uid + 1))) : ma2_strdup(uid + 1);
        json_t *ret = json_string(part);
        free(part);
        return ret;
    }
    return json_string(uid);
}

/* == typing == */

/* _set_typing core: if client: with suppress(Exception): set_typing(timeout) */
static json_t *ma2_set_typing_core(ma2_ctx_t *ctx, const json_t *args, double timeout) {
    const json_t *client = ma2_client(args);
    if (ma2_truthy(client)) {
        json_t *a = json_object();
        json_set(a, "chat_id", json_copy(json_obj_get(args, "chat_id")));
        json_set(a, "timeout", json_number(timeout));
        json_t *r = ma2_call(ctx, "client.set_typing", a);
        if (ma2_is_raise(r)) {
            if (ma2_raise_is_base(r)) return r; /* suppress passes base */
            json_free(r); /* suppressed */
        } else {
            json_free(r);
        }
    }
    return json_null();
}

/* PoP: ma2_set_typing @ plugins/platforms/matrix/adapter.py:_set_typing */
static json_t *ma2_set_typing(ma2_ctx_t *ctx, const json_t *args) {
    double timeout = json_get_num(args, "timeout", 0);
    return ma2_set_typing_core(ctx, args, timeout);
}

/* PoP: ma2_send_typing @ plugins/platforms/matrix/adapter.py:send_typing */
static json_t *ma2_send_typing(ma2_ctx_t *ctx, const json_t *args) {
    return ma2_set_typing_core(ctx, args, 30000.0);
}

/* PoP: ma2_stop_typing @ plugins/platforms/matrix/adapter.py:stop_typing */
static json_t *ma2_stop_typing(ma2_ctx_t *ctx, const json_t *args) {
    return ma2_set_typing_core(ctx, args, 0.0);
}
