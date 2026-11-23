/*
 * tui_hosted_room_service_flow2.c — FLOW port of a SECOND disjoint tranche of
 * UNCLAIMED functions of tui_gateway/hosted_room_service.py (LIVE authority,
 * py3.14 quarry). Self-contained: libjson only. Every function is static
 * thrs2_*; the harness (#include "gateway/tui_hosted_room_service_flow2.c")
 * wires the ONE universal seam to scripted results and records an ordered
 * trace.
 *
 * This flow2 tranche ports 15 defs NONE of which were claimed by
 * tui_hosted_room_service_flow (that flow claimed 20: task_member_id,
 * _approvable_attempts, attempt_action, _member_of, _authority,
 * _grant_revoke_is_terminal, _hosted_room_turn_timeout_seconds, _hook,
 * disband_fence, route_publication_lock, _member_activity, _attempt_actions,
 * _stop_pending_refusal, _policy_snapshot, profile_homes, _route_statuses,
 * _events, _append_room_status, publish_terminal, peer_admission_receipt).
 *
 * PoP ANNOTATIONS (one immediately above each static port):
 *   live_room_services, _member_is_peer, _owned_authority, route_authority,
 *   bindings, _list_tasks, _turn_lock, start, stop, wakeup, _save_link,
 *   _dispatch_approval, room_profiles, _make_rpc, _set_route_status
 * (all @ tui_gateway/hosted_room_service.py:<live def name>).
 *
 * FLOW DOCTRINE: port the DECISION SEQUENCE / call order / state transitions /
 * error paths. Every live effect enters ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the core propagates it
 * exactly where the live try/except catches it. `except (TypeError, ValueError)`
 * uses the FULL builtin parent table so sibling exceptions (and the
 * BaseException trio) pass through; thrs2_is_base_exc is the explicit marker.
 * A MISSING seam key fail-closes: the harness delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None.
 *
 * Seam name table (args are the projected kwargs; null-valued args are dropped
 * by the harness BEFORE recording, identically on both sides). THIS IS THE
 * EXACT SET the C core calls and the oracle stubs fire (set equality):
 *   live_services.snapshot                 {}
 *   self._room                             {room_id}
 *   local_authority_gateway_id             {}
 *   list_rooms                             {}
 *   driver.list_tasks                      {room_id, status}
 *   acquire_turn_lock                      {root, profile}
 *   self.runtime.start                     {}
 *   self.runtime.stop                      {timeout}
 *   self.runtime.wakeup                    {}
 *   hosted_room_links.make_stored_link     {<link kwargs>}
 *   hosted_room_links.save_room_link       {stored}
 *   approve_receipt                        {task_id, execution_generation,
 *                                           request_id, choice, grant}
 *   rpc.approve                            {session_id, request_id, choice}
 *   self.local_profiles                    {}
 *   HostedRoomServerRPC                    {server, db_path, root, placement}
 *   mark_room_link_status                  {room_id, member_id, status}
 *
 * Fixture domains (documented exclusions): ASCII strings; str()/int() only on
 * scalars; attribute doubles for self/client/route/action/room; callable attrs
 * are {"__callable__": marker}; missing scalar attrs raise AttributeError with
 * the live SimpleNamespace type name; every seam is scripted (fail-closed).
 *
 * Ownership: seam results and returned json_t pointers are malloc'd, caller
 * frees; inputs are borrowed. A propagated raise dict is returned upward; the
 * harness normalizes it into {__raise__, msg[, code]}.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

/* ── universal seam + ordered trace ── */

typedef struct thrs2_trace { json_t *events; } thrs2_trace_t;

typedef struct thrs2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} thrs2_seams_t;

static void thrs2_trace_add(thrs2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void thrs2_trace_free(thrs2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *thrs2_call(const thrs2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *thrs2_call0(const thrs2_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = thrs2_call(s, name, a);
    json_free(a);
    return r;
}

/* ── JSON value helpers ── */

static bool thrs2_jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool thrs2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool thrs2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool thrs2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool thrs2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool thrs2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
/* A bare JSON-number node's scalar value (json_get_num takes an object key). */
static double thrs2_node_num(const json_t *v, double def) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
/* Read a SCALAR json string node directly (json_get_str is an object accessor). */
static const char *thrs2_jstr(const json_t *v, const char *def) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static char *thrs2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* Python str() over the documented scalar domain. */
static json_t *thrs2_str(const json_t *v) {
    if (!v) return json_string("None");
    switch (v->type) {
        case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return json_string(v->bool_val ? "True" : "False");
        case JSON_NULL:   return json_string("None");
        case JSON_NUMBER: {
            char *s;
            if (v->num_val == (double)(long long)v->num_val)
                s = thrs2_xsf("%.0f", v->num_val);
            else
                s = thrs2_xsf("%g", v->num_val);
            json_t *r = json_string(s);
            free(s);
            return r;
        }
        default: {
            char *s = json_serialize(v);
            json_t *r = json_string(s ? s : "");
            free(s);
            return r;
        }
    }
}

/* Python int() over the documented scalar domain. On failure sets *exc to a
 * real raise descriptor (ValueError / TypeError) and returns 0. */
static long long thrs2_int(const json_t *v, json_t **exc) {
    if (thrs2_is_num(v)) return (long long)v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (thrs2_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
                      *p == '\v' || *p == '\f')) p++;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        const char *digits = p;
        while (*p >= '0' && *p <= '9') p++;
        if (p > digits) {
            const char *tail = p;
            while (*tail && (*tail == ' ' || *tail == '\t' || *tail == '\n' ||
                             *tail == '\r' || *tail == '\v' || *tail == '\f')) tail++;
            if (*tail == '\0') {
                long long n = 0;
                for (const char *q = digits; q < p; q++) n = n * 10 + (*q - '0');
                return neg ? -n : n;
            }
        }
        char *msg = thrs2_xsf("invalid literal for int() with base 10: '%s'", s);
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("ValueError"));
        json_set(e, "msg", json_string(msg));
        free(msg);
        *exc = e;
        return 0;
    }
    const char *tn = (!v || v->type == JSON_NULL) ? "NoneType"
                   : (v->type == JSON_ARRAY) ? "list"
                   : (v->type == JSON_OBJECT) ? "dict" : "object";
    char *msg = thrs2_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    json_t *e = json_object();
    json_set(e, "__raise__", json_string("TypeError"));
    json_set(e, "msg", json_string(msg));
    free(msg);
    *exc = e;
    return 0;
}

/* ── exception model (FULL builtin parent hierarchy) ── */

static const char *thrs2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool thrs2_is_raise(const json_t *r) { return thrs2_raise_class(r) != NULL; }

static json_t *thrs2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *thrs2_raise_spec_fmt(const char *cls, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    char *buf = malloc((size_t)(n < 0 ? 1 : n + 1));
    if (buf) {
        if (n >= 0) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
        else buf[0] = '\0';
    }
    va_end(ap2);
    json_t *r = thrs2_raise_spec(cls, buf ? buf : "");
    free(buf);
    return r;
}

static bool thrs2_in_names(const char *k, const char *const *names) {
    for (size_t i = 0; names[i]; i++) if (!strcmp(k, names[i])) return true;
    return false;
}
/* Full CPython 3.14 builtin exception parent table. The BaseException trio
 * stays on the BaseException branch ONLY so `except Exception` cannot catch
 * it. Unknown custom kinds base on Exception (mirrors the oracle). */
static const char *thrs2_exc_parent(const char *k) {
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
        !strcmp(k, "GeneratorExit")) return "BaseException";
    if (!strcmp(k, "Exception")) return "BaseException";
    static const char *const EXC_CHILDREN[] = {
        "StopIteration", "StopAsyncIteration", "ArithmeticError", "AssertionError",
        "AttributeError", "BufferError", "EOFError", "ImportError", "LookupError",
        "MemoryError", "NameError", "OSError", "ReferenceError", "RuntimeError",
        "SyntaxError", "SystemError", "TypeError", "ValueError", "Warning", NULL
    };
    if (thrs2_in_names(k, EXC_CHILDREN)) return "Exception";
    static const char *const ARITH_CHILDREN[] = {
        "FloatingPointError", "OverflowError", "ZeroDivisionError", NULL
    };
    if (thrs2_in_names(k, ARITH_CHILDREN)) return "ArithmeticError";
    static const char *const IMPORT_CHILDREN[] = { "ModuleNotFoundError", "ZipImportError", NULL };
    if (thrs2_in_names(k, IMPORT_CHILDREN)) return "ImportError";
    static const char *const LOOKUP_CHILDREN[] = { "IndexError", "KeyError", NULL };
    if (thrs2_in_names(k, LOOKUP_CHILDREN)) return "LookupError";
    static const char *const OSERROR_CHILDREN[] = {
        "BlockingIOError", "ChildProcessError", "ConnectionError", "FileExistsError",
        "FileNotFoundError", "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError", NULL
    };
    if (thrs2_in_names(k, OSERROR_CHILDREN)) return "OSError";
    static const char *const CONN_CHILDREN[] = {
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", NULL
    };
    if (thrs2_in_names(k, CONN_CHILDREN)) return "ConnectionError";
    static const char *const RUNTIME_CHILDREN[] = {
        "NotImplementedError", "RecursionError", "PythonFinalizationError", NULL
    };
    if (thrs2_in_names(k, RUNTIME_CHILDREN)) return "RuntimeError";
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "_IncompleteInputError")) return "SyntaxError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    static const char *const UNICODE_CHILDREN[] = {
        "UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError", NULL
    };
    if (thrs2_in_names(k, UNICODE_CHILDREN)) return "UnicodeError";
    if (!strcmp(k, "JSONDecodeError")) return "ValueError";
    static const char *const WARNING_CHILDREN[] = {
        "UserWarning", "DeprecationWarning", "PendingDeprecationWarning", "SyntaxWarning",
        "RuntimeWarning", "FutureWarning", "ImportWarning", "UnicodeWarning",
        "BytesWarning", "EncodingWarning", "ResourceWarning", NULL
    };
    if (thrs2_in_names(k, WARNING_CHILDREN)) return "Warning";
    static const char *const NAME_CHILDREN[] = { "UnboundLocalError", NULL };
    if (thrs2_in_names(k, NAME_CHILDREN)) return "NameError";
    if (!strcmp(k, "BaseExceptionGroup")) return "BaseException";
    if (!strcmp(k, "ExceptionGroup")) return "BaseExceptionGroup";
    return "Exception";
}
static bool thrs2_caught(const json_t *r, const char *cls) {
    const char *k = thrs2_raise_class(r);
    if (k && !strcmp(k, "ExceptionGroup") && !strcmp(cls, "Exception")) return true;
    while (k) {
        if (!strcmp(k, cls)) return true;
        k = thrs2_exc_parent(k);
    }
    return false;
}
/* BaseException trio passes through any `except Exception` / sibling catch. */
static bool thrs2_is_base_exc(const json_t *r) {
    const char *k = thrs2_raise_class(r);
    return k && (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt") ||
                 !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException"));
}

/* Attribute-double helpers: a present null is a valid None; an ABSENT scalar
 * attribute raises AttributeError with the live SimpleNamespace type name. */
static json_t *thrs2_attr(const json_t *obj, const char *name, json_t **exc) {
    const json_t *v = json_obj_get(obj, name);
    if (!v) {
        *exc = thrs2_raise_spec_fmt("AttributeError",
            "'types.SimpleNamespace' object has no attribute '%s'", name);
        return NULL;
    }
    return json_copy(v);
}

/* Python str(v) for `v or ""` (truthy scalar -> str(v), else ""). Caller frees. */
static char *thrs2_or_empty_str(const json_t *v) {
    if (!thrs2_jtruth(v)) return strdup("");
    json_t *s = thrs2_str(v);
    const char *p = (s && s->type == JSON_STRING && s->str_val) ? s->str_val : "";
    char *out = strdup(p);
    json_free(s);
    return out ? out : strdup("");
}

/* membership of a string in a JSON array of strings (set/double projection). */
static bool thrs2_arr_has_str(const json_t *arr, const char *want) {
    if (!thrs2_is_arr(arr) || !want) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (thrs2_is_str(arr->c.items[i]) && arr->c.items[i]->str_val &&
            !strcmp(arr->c.items[i]->str_val, want)) return true;
    return false;
}

/* get a callable marker from obj[name] == {"__callable__": <string>}. */
static const char *thrs2_callable_marker(const json_t *obj, const char *name) {
    const json_t *v = json_obj_get(obj, name);
    if (thrs2_is_obj(v) && v->c.count == 1) {
        const json_t *m = json_obj_get(v, "__callable__");
        if (m && m->type == JSON_STRING) return m->str_val;
    }
    return NULL;
}

/* local projection of the live module-level _authority(room):
 *   (str(room["authority_gateway_id"]), int(room["authority_epoch"]))
 * Returns a 2-element [str, number] json array; KeyError/ValueError via *exc. */
static json_t *thrs2_authority(const json_t *room, json_t **exc) {
    const json_t *gid = json_obj_get(room, "authority_gateway_id");
    if (!gid) { *exc = thrs2_raise_spec("KeyError", "'authority_gateway_id'"); return NULL; }
    const json_t *ep = json_obj_get(room, "authority_epoch");
    if (!ep) { *exc = thrs2_raise_spec("KeyError", "'authority_epoch'"); return NULL; }
    json_t *ie = NULL;
    long long epoch = thrs2_int(ep, &ie);
    if (ie) { *exc = ie; return NULL; }
    json_t *out = json_array();
    json_append(out, thrs2_str(gid));
    json_append(out, json_number((double)epoch));
    return out;
}


/* ── ports: module-level functions ── */

/* PoP: thrs2_live_room_services @ tui_gateway/hosted_room_service.py:live_room_services */
static json_t *thrs2_live_room_services(const thrs2_seams_t *s, json_t **exc) {
    /* with _live_services_lock: return list(_live_services) — the lock is inert.
     * The WeakSet double's __iter__ is iter(seam() or []), fired ONCE; the live
     * return is list() of that iterable. Emit the FULL Python list() semantics
     * on the scripted value instead of returning the raw seam node:
     *   falsy -> [], str -> list of 1-char strings, list -> copy,
     *   dict -> list of keys, int/float/bool -> TypeError not iterable. */
    json_t *r = thrs2_call0(s, "live_services.snapshot");
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    if (!thrs2_jtruth(r)) { json_free(r); return json_array(); }
    if (thrs2_is_arr(r)) return r;               /* list(list) == same sequence */
    if (thrs2_is_str(r)) {
        json_t *out = json_array();
        const char *p = r->str_val ? r->str_val : "";
        for (; *p; p++) {
            char one[2] = { *p, '\0' };
            json_append(out, json_string(one));
        }
        json_free(r);
        return out;
    }
    if (thrs2_is_obj(r)) {
        json_t *out = json_array();
        for (size_t i = 0; i < r->c.count; i++)
            json_append(out, json_string(r->c.keys[i] ? r->c.keys[i] : ""));
        json_free(r);
        return out;
    }
    {   /* truthy scalar but not iterable */
        const char *tn = (r->type == JSON_BOOL) ? "bool"
                       : (r->num_val == (double)(long long)r->num_val) ? "int" : "float";
        char *msg = thrs2_xsf("'%s' object is not iterable", tn);
        json_free(r);
        *exc = thrs2_raise_spec("TypeError", msg);
        free(msg);
        return NULL;
    }
}

/* ── ports: HostedRoomService methods (self is a borrowed JSON object) ── */

/* PoP: thrs2_member_is_peer @ tui_gateway/hosted_room_service.py:_member_is_peer */
static json_t *thrs2_member_is_peer(const thrs2_seams_t *s, const json_t *room_id,
                                    const json_t *member_id, json_t **exc) {
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(room_id));
    json_t *room = thrs2_call(s, "self._room", a);
    json_free(a);
    if (thrs2_is_raise(room)) { *exc = room; return NULL; }
    bool result = false;
    const json_t *members = json_obj_get(room, "members");
    if (thrs2_is_arr(members)) {
        const char *want = thrs2_jstr(member_id, "");
        for (size_t i = 0; i < members->c.count; i++) {
            const json_t *m = members->c.items[i];
            if (!thrs2_is_obj(m)) continue;  /* isinstance(m, Mapping) */
            const json_t *mv = json_obj_get(m, "member_id");
            if (!thrs2_jtruth(mv)) mv = json_obj_get(m, "profile");
            char *ms = thrs2_or_empty_str(mv);
            bool eq = !strcmp(ms, want);
            free(ms);
            if (eq) {
                const json_t *target = json_obj_get(m, "target");
                result = thrs2_is_obj(target) &&
                    !strcmp(thrs2_jstr(json_obj_get(target, "kind"), ""), "peer");
                break;
            }
        }
    }
    json_free(room);
    return json_bool(result);
}

/* PoP: thrs2_owned_authority @ tui_gateway/hosted_room_service.py:_owned_authority */
static json_t *thrs2_owned_authority(const thrs2_seams_t *s, const json_t *room_id, json_t **exc) {
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(room_id));
    json_t *room = thrs2_call(s, "self._room", a);
    json_free(a);
    if (thrs2_is_raise(room)) { *exc = room; return NULL; }
    json_t *auth = thrs2_authority(room, exc);   /* KeyError / ValueError */
    json_free(room);
    if (*exc) return NULL;
    json_t *lid = thrs2_call0(s, "local_authority_gateway_id");
    if (thrs2_is_raise(lid)) { json_free(auth); *exc = lid; return NULL; }
    json_t *lids = thrs2_str(lid);
    const char *gid = thrs2_jstr(json_get(auth, 0), "");
    bool conflict = strcmp(gid, lids->str_val ? lids->str_val : "") != 0;
    json_free(lids);
    json_free(lid);
    if (conflict) {
        json_free(auth);
        *exc = thrs2_raise_spec("AuthorityConflictError",
                                "This Group Chat is managed by another gateway.");
        return NULL;
    }
    return auth;
}

/* PoP: thrs2_route_authority @ tui_gateway/hosted_room_service.py:route_authority */
static json_t *thrs2_route_authority(const thrs2_seams_t *s, const json_t *self,
                                     const json_t *room_id, json_t **exc) {
    /* if room_id in self._disbanding_rooms: raise AuthorityRefusal(reason) — before any seam. */
    if (thrs2_arr_has_str(json_obj_get(self, "_disbanding_rooms"), thrs2_jstr(room_id, ""))) {
        *exc = thrs2_raise_spec("AuthorityRefusal", "room_disbanding_retry");
        return NULL;
    }
    return thrs2_owned_authority(s, room_id, exc);
}

/* PoP: thrs2_bindings @ tui_gateway/hosted_room_service.py:bindings */
/* One element of `for room in <list_rooms result>`: evaluate the generator's
 * filter str(room["authority_gateway_id"]) == local BEFORE the element expr,
 * then HostedRoomBinding(str(room["room_id"]), *_authority(room)) — the ctor's
 * str(room["room_id"]) is evaluated before _authority(room)'s
 * int(room["authority_epoch"]). A non-mapping element is subscripted with a
 * str key and raises exactly as Python does. */
static bool thrs2_binding_from_room(json_t *out, const json_t *room,
                                    const char *lid_s, json_t **exc) {
    if (!thrs2_is_obj(room)) {
        if (thrs2_is_str(room)) {
            *exc = thrs2_raise_spec("TypeError",
                                    "string indices must be integers, not 'str'");
        } else {
            const char *tn = thrs2_is_null(room) ? "NoneType"
                           : (room->type == JSON_BOOL) ? "bool"
                           : (room->num_val == (double)(long long)room->num_val) ? "int" : "float";
            char *msg = thrs2_xsf("'%s' object is not subscriptable", tn);
            *exc = thrs2_raise_spec("TypeError", msg);
            free(msg);
        }
        return false;
    }
    const json_t *gidv = json_obj_get(room, "authority_gateway_id");
    if (!gidv) { *exc = thrs2_raise_spec("KeyError", "'authority_gateway_id'"); return false; }
    json_t *gids = thrs2_str(gidv);
    if (strcmp(gids->str_val ? gids->str_val : "", lid_s) != 0) {
        json_free(gids);
        return true;                     /* filtered out by the generator `if` */
    }
    const json_t *rid = json_obj_get(room, "room_id");
    if (!rid) { json_free(gids); *exc = thrs2_raise_spec("KeyError", "'room_id'"); return false; }
    json_t *rids = thrs2_str(rid);
    const json_t *ep = json_obj_get(room, "authority_epoch");
    if (!ep) { json_free(rids); json_free(gids);
               *exc = thrs2_raise_spec("KeyError", "'authority_epoch'"); return false; }
    json_t *ie = NULL;
    long long epoch = thrs2_int(ep, &ie);
    if (ie) { json_free(rids); json_free(gids); *exc = ie; return false; }
    json_t *binding = json_array();
    json_append(binding, rids);
    json_append(binding, gids);
    json_append(binding, json_number((double)epoch));
    json_append(out, binding);
    return true;
}

static json_t *thrs2_bindings(const thrs2_seams_t *s, json_t **exc) {
    json_t *lid = thrs2_call0(s, "local_authority_gateway_id");
    if (thrs2_is_raise(lid)) { *exc = lid; return NULL; }
    json_t *rooms = thrs2_call0(s, "list_rooms");
    if (thrs2_is_raise(rooms)) { json_free(lid); *exc = rooms; return NULL; }
    json_t *out = json_array();
    const char *lid_s = thrs2_jstr(lid, "");
    bool ok = true;
    if (thrs2_is_arr(rooms)) {
        for (size_t i = 0; i < rooms->c.count && ok; i++)
            ok = thrs2_binding_from_room(out, rooms->c.items[i], lid_s, exc);
    } else if (thrs2_is_obj(rooms)) {
        /* `for room in <dict>` yields its KEYS (strings); subscripting one with
         * a str key raises TypeError, exactly as live does. */
        for (size_t i = 0; i < rooms->c.count && ok; i++) {
            json_t *key = json_string(rooms->c.keys[i] ? rooms->c.keys[i] : "");
            ok = thrs2_binding_from_room(out, key, lid_s, exc);
            json_free(key);
        }
    } else if (thrs2_is_str(rooms)) {
        const char *p = rooms->str_val ? rooms->str_val : "";
        for (; *p && ok; p++) {
            char one[2] = { *p, '\0' };
            json_t *ch = json_string(one);
            ok = thrs2_binding_from_room(out, ch, lid_s, exc);
            json_free(ch);
        }
    } else {
        /* None / number / bool: not iterable at all. */
        const char *tn = thrs2_is_null(rooms) ? "NoneType"
                       : (rooms->type == JSON_BOOL) ? "bool"
                       : (rooms->num_val == (double)(long long)rooms->num_val) ? "int" : "float";
        char *msg = thrs2_xsf("'%s' object is not iterable", tn);
        *exc = thrs2_raise_spec("TypeError", msg);
        free(msg);
        ok = false;
    }
    json_free(rooms);
    json_free(lid);
    if (!ok) { json_free(out); return NULL; }
    return out;
}

/* PoP: thrs2_list_tasks @ tui_gateway/hosted_room_service.py:_list_tasks */
static json_t *thrs2_list_tasks(const thrs2_seams_t *s, const json_t *room_id,
                                const json_t *statuses, json_t **exc) {
    json_t *out = json_array();
    if (thrs2_is_arr(statuses)) {
        for (size_t i = 0; i < statuses->c.count; i++) {
            json_t *a = json_object();
            json_set(a, "room_id", json_copy(room_id));
            json_set(a, "status", json_copy(statuses->c.items[i]));
            json_t *r = thrs2_call(s, "driver.list_tasks", a);
            json_free(a);
            if (thrs2_is_raise(r)) { json_free(out); *exc = r; return NULL; }
            if (thrs2_is_arr(r))
                for (size_t j = 0; j < r->c.count; j++) json_append(out, json_copy(r->c.items[j]));
            json_free(r);
        }
    }
    return out;
}

/* str(PurePosixPath(p)): collapse duplicate '/', drop '.' components, keep
 * '..', strip trailing separators (except the root), empty -> ".". Caller frees. */
static char *thrs2_posix_norm(const char *p) {
    if (!p || !*p) return strdup(".");
    size_t n = strlen(p);
    size_t lead = 0;
    while (lead < n && p[lead] == '/') lead++;
    const char *root = (lead == 2) ? "//" : (lead > 0 ? "/" : "");
    char *out = malloc(n + strlen(root) + 2);
    if (!out) return strdup(".");
    size_t k = 0;
    for (const char *q = root; *q; ) out[k++] = *q++;
    size_t i = lead;
    while (i < n) {
        while (i < n && p[i] == '/') i++;
        size_t st = i;
        while (i < n && p[i] != '/') i++;
        if (i == st) break;
        size_t len = i - st;
        if (len == 1 && p[st] == '.') continue;
        if (k > 0 && out[k - 1] != '/') out[k++] = '/';
        memcpy(out + k, p + st, len);
        k += len;
    }
    out[k] = '\0';
    if (k == 0) { free(out); return strdup("."); }
    return out;
}

/* PoP: thrs2_turn_lock @ tui_gateway/hosted_room_service.py:_turn_lock */
static json_t *thrs2_turn_lock(const thrs2_seams_t *s, const char *root,
                               const char *profile, json_t **exc) {
    char *rootn = thrs2_posix_norm(root);
    json_t *a = json_object();
    json_set(a, "root", json_string(rootn));
    free(rootn);
    json_set(a, "profile", json_string(profile));
    json_t *r = thrs2_call(s, "acquire_turn_lock", a);
    json_free(a);
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    return r;
}

/* PoP: thrs2_start @ tui_gateway/hosted_room_service.py:start */
static json_t *thrs2_start(const thrs2_seams_t *s, json_t **exc) {
    json_t *r = thrs2_call0(s, "self.runtime.start");
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    json_free(r);
    return json_null();
}

/* PoP: thrs2_stop @ tui_gateway/hosted_room_service.py:stop */
static json_t *thrs2_stop(const thrs2_seams_t *s, double timeout, json_t **exc) {
    json_t *a = json_object();
    json_set(a, "timeout", json_number(timeout));
    json_t *r = thrs2_call(s, "self.runtime.stop", a);
    json_free(a);
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    return r;
}

/* PoP: thrs2_wakeup @ tui_gateway/hosted_room_service.py:wakeup */
static json_t *thrs2_wakeup(const thrs2_seams_t *s, json_t **exc) {
    json_t *r = thrs2_call0(s, "self.runtime.wakeup");
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    json_free(r);
    return json_null();
}

/* PoP: thrs2_save_link @ tui_gateway/hosted_room_service.py:_save_link */
static json_t *thrs2_save_link(const thrs2_seams_t *s, const json_t *link, json_t **exc) {
    json_t *a = thrs2_is_obj(link) ? json_copy(link) : json_object();
    json_t *stored = thrs2_call(s, "hosted_room_links.make_stored_link", a);
    json_free(a);
    if (thrs2_is_raise(stored)) { *exc = stored; return NULL; }
    json_t *a2 = json_object();
    json_set(a2, "stored", stored);
    json_t *r = thrs2_call(s, "hosted_room_links.save_room_link", a2);
    json_free(a2);
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    json_free(r);
    return json_null();
}

/* PoP: thrs2_dispatch_approval @ tui_gateway/hosted_room_service.py:_dispatch_approval */
static json_t *thrs2_dispatch_approval(const thrs2_seams_t *s, const json_t *route,
        const json_t *client, const json_t *action, const char *task_id,
        double execution_generation, const char *request_id, const char *choice, json_t **exc) {
    json_t *r;
    bool have_approve = thrs2_callable_marker(client, "approve_receipt") != NULL;
    if (thrs2_is_obj(route) && have_approve) {
        json_t *a = json_object();
        json_set(a, "task_id", json_string(task_id));
        json_set(a, "execution_generation", json_number(execution_generation));
        json_set(a, "request_id", json_string(request_id));
        json_set(a, "choice", json_string(choice));
        json_set(a, "grant", json_copy(json_obj_get(route, "grant")));
        r = thrs2_call(s, "approve_receipt", a);
        json_free(a);
    } else {
        char *sid = thrs2_or_empty_str(json_obj_get(action, "session_id"));
        if (sid[0] == '\0') {
            free(sid);
            *exc = thrs2_raise_spec("RuntimeError", "local room approval identity is unavailable");
            return NULL;
        }
        json_t *a = json_object();
        json_set(a, "session_id", json_string(sid));
        json_set(a, "request_id", json_string(request_id));
        json_set(a, "choice", json_string(choice));
        free(sid);
        r = thrs2_call(s, "rpc.approve", a);
        json_free(a);
    }
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    if (thrs2_is_null(r)) { *exc = thrs2_raise_spec("RuntimeError", "room approval target is unavailable"); return NULL; }
    return r;
}

/* PoP: thrs2_room_profiles @ tui_gateway/hosted_room_service.py:room_profiles */
static json_t *thrs2_room_profiles(const thrs2_seams_t *s, json_t **exc) {
    json_t *r = thrs2_call0(s, "self.local_profiles");
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    return r;
}

/* PoP: thrs2_make_rpc @ tui_gateway/hosted_room_service.py:_make_rpc */
static json_t *thrs2_make_rpc(const thrs2_seams_t *s, const json_t *self,
                              const json_t *server, json_t **exc) {
    json_t *a = json_object();
    json_set(a, "server", json_copy(server));
    json_set(a, "db_path", json_string(json_get_str(self, "db_path", "")));
    json_set(a, "root", json_string(json_get_str(self, "root", "")));
    json_set(a, "placement", json_string(json_get_str(self, "placement", "")));
    json_t *r = thrs2_call(s, "HostedRoomServerRPC", a);
    json_free(a);
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    return r;
}

/* PoP: thrs2_set_route_status @ tui_gateway/hosted_room_service.py:_set_route_status */
static json_t *thrs2_set_route_status(const thrs2_seams_t *s, const json_t *self,
        const json_t *room_id, const json_t *member_id, const json_t *status, json_t **exc) {
    /* with _policy_lock, _state_lock (inert): if current == status: return None */
    bool equal = false;
    const json_t *states = json_obj_get(self, "_peer_route_status");
    if (thrs2_is_arr(states)) {
        const char *want_room = thrs2_jstr(room_id, "");
        const char *want_mem = thrs2_jstr(member_id, "");
        for (size_t i = 0; i < states->c.count; i++) {
            const json_t *e = states->c.items[i];
            if (!strcmp(thrs2_jstr(json_obj_get(e, "room_id"), ""), want_room) &&
                !strcmp(thrs2_jstr(json_obj_get(e, "member_id"), ""), want_mem)) {
                equal = !strcmp(thrs2_jstr(json_obj_get(e, "status"), ""),
                                thrs2_jstr(status, ""));
                break;
            }
        }
    }
    if (equal) return json_null();
    json_t *a = json_object();
    json_set(a, "room_id", json_copy(room_id));
    json_set(a, "member_id", json_copy(member_id));
    json_set(a, "status", json_copy(status));
    json_t *r = thrs2_call(s, "mark_room_link_status", a);
    json_free(a);
    if (thrs2_is_raise(r)) { *exc = r; return NULL; }
    json_free(r);
    return json_null();
}
