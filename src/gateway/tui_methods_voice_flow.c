/*
 * tui_methods_voice_flow.c -- FLOW port of the TUI voice / TTS / wake-word
 * method surface (tui_gateway/methods_voice.py).
 *
 * Ports 27 UNCLAIMED functions of the voice method family. Every PoP below
 * credits the REAL defining module, tui_gateway/methods_voice.py. Claimed
 * siblings and server globals (_ok/_err/_load_cfg/_emit/bind_transport/...)
 * are SEAMED; ported siblings call each other in-line so their own seam
 * events appear in the trace in live evaluation order.
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message
 * composition, seam call order, import/except domains and state mutation --
 * not just leaf outputs. Every live external call enters through ONE
 * universal seam: tmv_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmv_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   import                       from <module> import <names>  {module,fromlist}
 *   current_transport            current_transport() -> transport | null
 *   bind_transport               bind_transport(transport) -> token
 *   emit                         _emit(event, sid, payload)
 *   reset_transport              reset_transport(token)
 *   wake_resume_if_owner         _wake_resume_if_owner(owner) -> bool
 *   env_get                      os.environ.get(key, default) -> str
 *   env_set                      os.environ[key] = value
 *   local_voice_enabled          session_config.local_voice_enabled(sid)
 *   set_local_voice_mode         session_config.set_local_voice_mode(sid,"off")
 *   stop_continuous              voice.stop_continuous()
 *   is_continuous_active         voice.is_continuous_active() -> bool
 *   tts_stream_stop              _tts_stream_stop(user_barge=)   [seamed sibling]
 *   tts_lease_async              _tts_lease_async(lease, active)
 *   arm_full_duplex_listener     _arm_full_duplex_listener()
 *   load_cfg                     _load_cfg()
 *   ok                           _ok(rid, result)
 *   err                          _err(rid, code, msg)
 *   stop_listening               wake_word.stop_listening(owner=)
 *   save_config_value            cli.save_config_value("wake_word.enabled",v)
 *   is_voice_stop_phrase         voice_mode_transcript.is_voice_stop_phrase(text)
 *   check_voice_requirements     voice_mode.check_voice_requirements()
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
 *   BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception site.
 * - Transport identity is the marker string; two transports are "is" when
 *   their marker strings are equal (fixtures use a per-id registry).
 * - Tuples project to lists; integral floats render as ints; paths are POSIX;
 *   str()/strip()/lower()/isdigit() fold the ASCII fixture corpus only.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 *
 * Ownership: tmv_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value. Seam results are caller-freed.
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

/* -- universal seam context -- */
typedef struct tmv_ctx {
    json_t *state;      /* owned; module-global mirror, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmv_ctx_t;

/* -- basic helpers -- */
static bool tmv_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *tmv_raise_kind(const json_t *v) {
    return tmv_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool tmv_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *tmv_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *tmv_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static bool tmv_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *tmv_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *tmv_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmv_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmv_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* -- full builtin exception hierarchy (py3.14.9) -- */
typedef struct { const char *name; const char *parent; } tmv_exc_row_t;
static const tmv_exc_row_t TMV_EXC_PARENTS[] = {
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"IOError", "OSError"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};
#define TMV_EXC_PARENT_COUNT (sizeof(TMV_EXC_PARENTS)/sizeof(TMV_EXC_PARENTS[0]))
static const char *tmv_exc_parent(const char *kind) {
    if (!kind) return NULL;
    for (size_t i = 0; i < TMV_EXC_PARENT_COUNT; i++)
        if (!strcmp(TMV_EXC_PARENTS[i].name, kind)) return TMV_EXC_PARENTS[i].parent;
    return NULL;
}
/* issubclass(kind, target) over the single-parent table. Unknown kinds are
 * synthetic Exception subclasses (the oracle's make_exc fallback). */
static bool tmv_exc_is_a(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (!strcmp(cur, target)) return true;
        if (!strcmp(cur, "BaseException") || !strcmp(cur, "object")) return false;
        const char *p = tmv_exc_parent(cur);
        if (!p) return !strcmp(target, "Exception");
        cur = p;
    }
    return false;
}
/* except Exception: swallows Exception subclasses; BaseException-only kinds
 * pass through. */
static bool tmv_caught_by_exception(const json_t *r) {
    return tmv_exc_is_a(tmv_raise_kind(r), "Exception");
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tmv_call(tmv_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tmv_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmv_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT)
                ret = tmv_err_spec(json_get_str(raise, "type", "RuntimeError"),
                                   json_get_str(raise, "message", ""));
            else {
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

/* -- args builders (values borrowed; copied into the args object) -- */
static json_t *tmv_args0(void) { return json_object(); }
static void tmv_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmv_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmv_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmv_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* -- state helpers -- */
static json_t *tmv_state_get(tmv_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static const char *tmv_state_str(tmv_ctx_t *ctx, const char *key, const char *def) {
    const json_t *v = tmv_state_get(ctx, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static void tmv_state_set_str(tmv_ctx_t *ctx, const char *key, const char *val) {
    json_set(ctx->state, key, json_string(val ? val : ""));
}
static void tmv_state_set_null(tmv_ctx_t *ctx, const char *key) {
    json_set(ctx->state, key, json_null());
}
static bool tmv_transport_eq(const json_t *a, const json_t *b) {
    bool an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}

/* -- seam wrappers -- */
static json_t *tmv_import(tmv_ctx_t *ctx, const char *module,
                          const char *const *names, size_t n) {
    json_t *a = tmv_args0();
    tmv_asets(a, "module", module);
    json_t *fl = json_array();
    for (size_t i = 0; i < n; i++) json_append(fl, json_string(names[i] ? names[i] : ""));
    json_set(a, "fromlist", fl);
    return tmv_call(ctx, "import", a);
}
static json_t *tmv_env_get(tmv_ctx_t *ctx, const char *key, const char *def) {
    json_t *a = tmv_args0();
    tmv_asets(a, "key", key);
    tmv_asets(a, "default", def);
    return tmv_call(ctx, "env_get", a);
}
static json_t *tmv_env_set(tmv_ctx_t *ctx, const char *key, const char *value) {
    json_t *a = tmv_args0();
    tmv_asets(a, "key", key);
    tmv_asets(a, "value", value);
    return tmv_call(ctx, "env_set", a);
}
static json_t *tmv_emit_call(tmv_ctx_t *ctx, const char *event, const char *sid,
                             const json_t *payload) {
    json_t *a = tmv_args0();
    tmv_asets(a, "event", event);
    tmv_asets(a, "sid", sid);
    tmv_aset(a, "payload", payload);
    return tmv_call(ctx, "emit", a);
}
static json_t *tmv_ok(tmv_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmv_args0();
    tmv_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null());
    return tmv_call(ctx, "ok", a);
}
static json_t *tmv_err(tmv_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tmv_args0();
    tmv_aset(a, "rid", rid);
    tmv_asetn(a, "code", (double)code);
    tmv_asets(a, "message", msg);
    return tmv_call(ctx, "err", a);
}

/* -- forward declarations of ported functions -- */
static json_t *tmv_caller_transport(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_emit(tmv_ctx_t *, const json_t *);
static json_t *tmv_resume_voice_wake(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_mode_enabled(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_tts_enabled(tmv_ctx_t *, const json_t *);
static json_t *tmv_session_voice_enabled(tmv_ctx_t *, const json_t *);
static json_t *tmv_set_session_voice_off(tmv_ctx_t *, const json_t *);
static json_t *tmv_capture_live(tmv_ctx_t *, const json_t *);
static json_t *tmv_end_voice_chat(tmv_ctx_t *, const json_t *);
static json_t *tmv_end_session_voice_chat(tmv_ctx_t *, const json_t *);
static json_t *tmv_running_sessions(tmv_ctx_t *, const json_t *);
static json_t *tmv_any_session_running(tmv_ctx_t *, const json_t *);
static json_t *tmv_arm_barge_listener_if_enabled(tmv_ctx_t *, const json_t *);
static json_t *tmv_fd_barge_params(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_cfg_dict(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_cfg_number(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_status_payload(tmv_ctx_t *, const json_t *);
static json_t *tmv_wake_owner_snapshot(tmv_ctx_t *, const json_t *);
static json_t *tmv_release_wake_for_transport(tmv_ctx_t *, const json_t *);
static json_t *tmv_release_gateway_wake_owner(tmv_ctx_t *, const json_t *);
static json_t *tmv_persist_wake_enabled(tmv_ctx_t *, const json_t *);
static json_t *tmv_owner_result(tmv_ctx_t *, const json_t *);
static json_t *tmv_deliver_fd_transcript(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_toggle_status(tmv_ctx_t *, const json_t *);
static json_t *tmv_set_voice_tts(tmv_ctx_t *, const json_t *);
static json_t *tmv_voice_toggle_tts(tmv_ctx_t *, const json_t *);
static json_t *tmv_vr_transcript(tmv_ctx_t *, const json_t *);

/* ========================================================================
 * PORTED FUNCTIONS -- one PoP annotation each
 * ======================================================================== */

/* PoP: _caller_transport @ tui_gateway/methods_voice.py:_caller_transport */
static json_t *tmv_caller_transport(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = tmv_call(ctx, "current_transport", tmv_args0());
    if (tmv_is_raise(r)) return r;
    if (!tmv_truthy(r)) { json_free(r); return json_string("stdio"); }
    return r;
}

/* PoP: _voice_emit @ tui_gateway/methods_voice.py:_voice_emit */
static json_t *tmv_voice_emit(tmv_ctx_t *ctx, const json_t *args) {
    const char *event = json_get_str(args, "event", "");
    const json_t *payload = json_obj_get(args, "payload");
    const char *sid = tmv_state_str(ctx, "voice_event_sid", "");
    json_t *transport = tmv_state_get(ctx, "voice_event_transport");
    json_t *token = NULL, *r = NULL;
    if (transport && transport->type != JSON_NULL) {
        json_t *a = tmv_args0();
        tmv_aset(a, "transport", transport);
        token = tmv_call(ctx, "bind_transport", a);
        if (tmv_is_raise(token)) return token;
    }
    r = tmv_emit_call(ctx, event, sid, payload);
    /* Python: _emit is the try body, reset_transport is the finally.
     * A raise in the finally overrides a raise from _emit; otherwise an
     * _emit raise propagates (the call is in statement position only for
     * the success path, which returns None). */
    if (token && token->type != JSON_NULL) {
        json_t *a = tmv_args0();
        tmv_aset(a, "token", token);
        json_t *rr = tmv_call(ctx, "reset_transport", a);
        if (tmv_is_raise(rr)) { json_free(token); json_free(r); return rr; }
        json_free(rr);
    }
    json_free(token);
    if (tmv_is_raise(r)) return r;  /* _emit raise propagates through finally */
    json_free(r);
    return json_null();
}

/* PoP: _resume_voice_wake @ tui_gateway/methods_voice.py:_resume_voice_wake */
static json_t *tmv_resume_voice_wake(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *owner = tmv_state_get(ctx, "voice_wake_owner");
    bool have = owner && owner->type != JSON_NULL;
    json_t *ownercopy = have ? json_copy(owner) : NULL;
    tmv_state_set_null(ctx, "voice_wake_owner");
    if (have) {
        json_t *a = tmv_args0();
        tmv_aset(a, "owner", ownercopy);
        json_t *r = tmv_call(ctx, "wake_resume_if_owner", a);
        json_free(ownercopy);
        if (tmv_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: _voice_mode_enabled @ tui_gateway/methods_voice.py:_voice_mode_enabled */
static json_t *tmv_voice_mode_enabled(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *v = tmv_env_get(ctx, "HERMES_VOICE", "");
    if (tmv_is_raise(v)) return v;
    char *s = tmv_strip(tmv_str(v) ? tmv_str(v) : "");
    bool hit = s && !strcmp(s, "1");
    free(s);
    json_free(v);
    return json_bool(hit);
}

/* PoP: _voice_tts_enabled @ tui_gateway/methods_voice.py:_voice_tts_enabled */
static json_t *tmv_voice_tts_enabled(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *v = tmv_env_get(ctx, "HERMES_VOICE_TTS", "");
    if (tmv_is_raise(v)) return v;
    char *s = tmv_strip(tmv_str(v) ? tmv_str(v) : "");
    bool hit = s && !strcmp(s, "1");
    free(s);
    json_free(v);
    return json_bool(hit);
}

/* PoP: _session_voice_enabled @ tui_gateway/methods_voice.py:_session_voice_enabled */
static json_t *tmv_session_voice_enabled(tmv_ctx_t *ctx, const json_t *args) {
    const char *const NAMES[] = {"local_voice_enabled"};
    json_t *held = NULL;
    json_t *imp = tmv_import(ctx, "gateway.session_config", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (!tmv_exc_is_a(tmv_raise_kind(imp), "ImportError")) return imp;
        json_free(imp);
    } else {
        json_free(imp);
        json_t *a = tmv_args0();
        tmv_aset(a, "session_id", json_obj_get(args, "session_id"));
        held = tmv_call(ctx, "local_voice_enabled", a);
        if (tmv_is_raise(held)) {
            if (!tmv_exc_is_a(tmv_raise_kind(held), "ImportError")) return held;
            json_free(held);
            held = NULL;
        }
    }
    if (!held || held->type == JSON_NULL) {
        json_free(held);
        return tmv_voice_mode_enabled(ctx, NULL);
    }
    return held;  /* live returns held verbatim (not boolified) */
}

/* PoP: _set_session_voice_off @ tui_gateway/methods_voice.py:_set_session_voice_off */
static json_t *tmv_set_session_voice_off(tmv_ctx_t *ctx, const json_t *args) {
    const char *const NAMES[] = {"set_local_voice_mode"};
    json_t *imp = tmv_import(ctx, "gateway.session_config", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (!tmv_exc_is_a(tmv_raise_kind(imp), "ImportError")) return imp;
        json_free(imp);
        return json_bool(false);
    }
    json_free(imp);
    json_t *a = tmv_args0();
    tmv_aset(a, "session_id", json_obj_get(args, "session_id"));
    tmv_asets(a, "mode", "off");
    json_t *r = tmv_call(ctx, "set_local_voice_mode", a);
    if (tmv_is_raise(r)) {
        if (!tmv_exc_is_a(tmv_raise_kind(r), "ImportError")) return r;
        json_free(r);
        return json_bool(false);
    }
    bool hit = r && r->type == JSON_BOOL && r->bool_val;
    json_free(r);
    return json_bool(hit);
}

/* PoP: _capture_live @ tui_gateway/methods_voice.py:_capture_live */
static json_t *tmv_capture_live(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    const char *const NAMES[] = {"is_continuous_active"};
    json_t *imp = tmv_import(ctx, "hermes_cli.voice", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (tmv_exc_is_a(tmv_raise_kind(imp), "ImportError")) {
            json_free(imp);
            return json_bool(false);
        }
        return imp;  /* call is outside the try: non-ImportError propagates */
    }
    json_free(imp);
    json_t *r = tmv_call(ctx, "is_continuous_active", tmv_args0());
    if (tmv_is_raise(r)) return r;
    bool hit = tmv_truthy(r);
    json_free(r);
    return json_bool(hit);
}

/* PoP: _end_voice_chat @ tui_gateway/methods_voice.py:_end_voice_chat */
static json_t *tmv_end_voice_chat(tmv_ctx_t *ctx, const json_t *args) {
    bool stop_loop = json_get_bool(args, "stop_loop", false);
    bool stop_tts = json_get_bool(args, "stop_tts", false);
    json_t *r = tmv_env_set(ctx, "HERMES_VOICE", "0");
    if (tmv_is_raise(r)) return r;
    json_free(r);
    r = tmv_env_set(ctx, "HERMES_VOICE_TTS", "0");
    if (tmv_is_raise(r)) return r;
    json_free(r);
    if (stop_loop) {
        const char *const NAMES[] = {"stop_continuous"};
        json_t *imp = tmv_import(ctx, "hermes_cli.voice", NAMES, 1);
        if (tmv_is_raise(imp)) {
            if (tmv_caught_by_exception(imp)) json_free(imp);
            else return imp;
        } else {
            json_free(imp);
            json_t *sc = tmv_call(ctx, "stop_continuous", tmv_args0());
            if (tmv_is_raise(sc)) {
                if (tmv_caught_by_exception(sc)) json_free(sc);
                else return sc;
            } else json_free(sc);
        }
    }
    if (stop_tts) {
        json_t *a = tmv_args0();
        tmv_asetb(a, "user_barge", false);
        json_t *st = tmv_call(ctx, "tts_stream_stop", a);
        if (tmv_is_raise(st)) {
            if (tmv_caught_by_exception(st)) json_free(st);
            else return st;
        } else json_free(st);
    }
    return json_null();
}

/* PoP: _end_session_voice_chat @ tui_gateway/methods_voice.py:_end_session_voice_chat */
static json_t *tmv_end_session_voice_chat(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *sid = json_obj_get(args, "session_id");
    bool stop_loop = json_get_bool(args, "stop_loop", false);
    bool stop_tts = json_get_bool(args, "stop_tts", false);
    json_t *a = tmv_args0();
    tmv_aset(a, "session_id", sid);
    json_t *off = tmv_set_session_voice_off(ctx, a);
    if (tmv_is_raise(off)) return off;
    if (!tmv_truthy(off)) {
        json_t *b = tmv_args0();
        tmv_asetb(b, "stop_loop", stop_loop);
        tmv_asetb(b, "stop_tts", stop_tts);
        json_t *r = tmv_end_voice_chat(ctx, b);
        json_free(off);
        return r;
    }
    json_free(off);
    if (stop_tts) {
        json_t *b = tmv_args0();
        tmv_asetb(b, "user_barge", false);
        json_t *st = tmv_call(ctx, "tts_stream_stop", b);
        if (tmv_is_raise(st)) {
            if (tmv_caught_by_exception(st)) json_free(st);
            else return st;
        } else json_free(st);
    }
    return json_null();
}

/* PoP: _running_sessions @ tui_gateway/methods_voice.py:_running_sessions */
static json_t *tmv_running_sessions(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *arr = json_array();
    json_t *sessions = tmv_state_get(ctx, "sessions");
    if (sessions && sessions->type == JSON_OBJECT) {
        for (size_t i = 0; i < sessions->c.count; i++) {
            const json_t *s = sessions->c.items[i];
            if (tmv_truthy(json_obj_get(s, "running")))
                json_append(arr, json_copy(s));
        }
    }
    return arr;
}

/* PoP: _any_session_running @ tui_gateway/methods_voice.py:_any_session_running */
static json_t *tmv_any_session_running(tmv_ctx_t *ctx, const json_t *args) {
    json_t *r = tmv_running_sessions(ctx, args);
    if (tmv_is_raise(r)) {  /* only ImportError-free leaves here; kept for parity */
        if (tmv_caught_by_exception(r)) { json_free(r); return json_bool(false); }
        return r;
    }
    bool hit = tmv_truthy(r);
    json_free(r);
    return json_bool(hit);
}

/* PoP: _arm_barge_listener_if_enabled @ tui_gateway/methods_voice.py:_arm_barge_listener_if_enabled */
static json_t *tmv_arm_barge_listener_if_enabled(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *v = tmv_voice_mode_enabled(ctx, NULL);
    if (tmv_is_raise(v)) return v;
    bool on = tmv_truthy(v);
    json_free(v);
    if (!on) return json_null();
    json_t *cfg = tmv_voice_cfg_dict(ctx, NULL);
    if (tmv_is_raise(cfg)) return cfg;
    bool barge = true;
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *b = json_obj_get(cfg, "barge_in");
        if (b) barge = tmv_truthy(b);
    }
    json_free(cfg);
    if (barge) {
        json_t *r = tmv_call(ctx, "arm_full_duplex_listener", tmv_args0());
        if (tmv_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* Python float() over the ASCII fixture domain. Returns a number or parks a
 * raise spec in *raise (caller frees). Value `or 0` is handled by caller. */
static json_t *tmv_pyfloat(const json_t *v, json_t **raise_out) {
    *raise_out = NULL;
    if (!v || v->type == JSON_NULL) {
        *raise_out = tmv_err_spec("TypeError", "float() argument must be a string or a real number");
        return NULL;
    }
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type != JSON_STRING) {
        *raise_out = tmv_err_spec("TypeError", "float() argument must be a string or a real number");
        return NULL;
    }
    const char *s = v->str_val ? v->str_val : "";
    char *end = NULL;
    double d = strtod(s, &end);
    if (end == s || (end && *end != '\0')) {
        char msg[200];
        snprintf(msg, sizeof msg, "could not convert string to float: '%s'", s);
        *raise_out = tmv_err_spec("ValueError", msg);
        return NULL;
    }
    return json_number(d);
}

/* PoP: _fd_barge_params @ tui_gateway/methods_voice.py:_fd_barge_params */
static json_t *tmv_fd_barge_params(tmv_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *cfg = args;
    json_t *out = json_array();
    /* num(lambda v: float(v or 0), "barge_in_threshold_multiplier", 0) */
    const json_t *mv = json_obj_get(cfg, "barge_in_threshold_multiplier");
    json_t *ra = NULL;
    /* conv(cfg.get(key, default)) with conv = float(v or 0): any falsy
     * scripted value (absent/null/0/""/False) becomes 0.0. */
    json_t *m = (mv && tmv_truthy(mv)) ? tmv_pyfloat(mv, &ra) : json_number(0.0);
    if (!m) { json_free(ra); m = json_number(0.0); }
    json_append(out, m);
    /* num(lambda v: int(float(v) * 1000), "barge_in_grace_seconds", 0.5) */
    const json_t *gv = json_obj_get(cfg, "barge_in_grace_seconds");
    json_t *g = NULL;
    if (gv) {
        g = tmv_pyfloat(gv, &ra);
        if (!g) { json_free(ra); g = tmv_pyfloat(json_number(0.5), &ra); }
    } else g = tmv_pyfloat(json_number(0.5), &ra);
    double gd = g ? g->num_val : 0.5;
    json_free(g);
    double ms = gd * 1000.0;
    long long iv = (long long)ms;  /* int() truncates toward zero */
    if (iv < 0) iv = 0;            /* max(0, ...) */
    json_append(out, json_number((double)iv));
    return out;
}

/* PoP: _voice_cfg_dict @ tui_gateway/methods_voice.py:_voice_cfg_dict */
static json_t *tmv_voice_cfg_dict(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cfg = tmv_call(ctx, "load_cfg", tmv_args0());
    if (tmv_is_raise(cfg)) return cfg;
    json_t *ret = json_object();
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(cfg, "voice");
        if (v && v->type == JSON_OBJECT) { json_free(ret); ret = json_copy(v); }
    }
    json_free(cfg);
    return ret;
}

/* PoP: _voice_cfg_number @ tui_gateway/methods_voice.py:_voice_cfg_number */
static json_t *tmv_voice_cfg_number(tmv_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = json_obj_get(args, "value");
    const json_t *d = json_obj_get(args, "default");
    if (v && (v->type == JSON_NUMBER)) return json_copy(v);
    return d ? json_copy(d) : json_null();
}

/* PoP: _voice_status_payload @ tui_gateway/methods_voice.py:_voice_status_payload */
static json_t *tmv_voice_status_payload(tmv_ctx_t *ctx, const json_t *args) {
    json_t *cfg = tmv_voice_cfg_dict(ctx, NULL);
    if (tmv_is_raise(cfg)) return cfg;
    const char *rk = "ctrl+b";
    if (cfg && cfg->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(cfg, "record_key");
        if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) rk = v->str_val;
    }
    char *rk_copy = tmv_strdup(rk);
    json_free(cfg);
    json_t *en = tmv_voice_mode_enabled(ctx, NULL);
    if (tmv_is_raise(en)) { free(rk_copy); return en; }
    json_t *tt = tmv_voice_tts_enabled(ctx, NULL);
    if (tmv_is_raise(tt)) { json_free(en); free(rk_copy); return tt; }
    json_t *out = json_object();
    json_set(out, "enabled", en);
    json_set(out, "record_key", json_string(rk_copy));
    json_set(out, "tts", tt);
    const json_t *extra = json_obj_get(args, "extra");
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    free(rk_copy);
    return out;
}

/* PoP: _wake_owner_snapshot @ tui_gateway/methods_voice.py:_wake_owner_snapshot */
static json_t *tmv_wake_owner_snapshot(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *arr = json_array();
    json_t *owner = tmv_state_get(ctx, "wake_owner_transport");
    json_append(arr, owner ? json_copy(owner) : json_null());
    const json_t *surf = tmv_state_get(ctx, "wake_owner_surface");
    json_append(arr, surf ? json_copy(surf) : json_null());
    return arr;
}

/* PoP: _release_wake_for_transport @ tui_gateway/methods_voice.py:_release_wake_for_transport */
static json_t *tmv_release_wake_for_transport(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *transport = json_obj_get(args, "transport");
    json_t *owner = tmv_state_get(ctx, "wake_owner_transport");
    if (!tmv_transport_eq(owner, transport)) return json_bool(false);
    tmv_state_set_null(ctx, "wake_owner_transport");
    tmv_state_set_str(ctx, "wake_owner_surface", "");
    const char *const NAMES[] = {"stop_listening"};
    json_t *imp = tmv_import(ctx, "tools.wake_word", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (tmv_caught_by_exception(imp)) json_free(imp);
        else return imp;
    } else {
        json_free(imp);
        json_t *a = tmv_args0();
        tmv_aset(a, "owner", transport);
        json_t *sl = tmv_call(ctx, "stop_listening", a);
        if (tmv_is_raise(sl)) {
            if (tmv_caught_by_exception(sl)) json_free(sl);
            else return sl;
        } else json_free(sl);
    }
    return json_bool(true);
}

/* PoP: _release_gateway_wake_owner @ tui_gateway/methods_voice.py:_release_gateway_wake_owner */
static json_t *tmv_release_gateway_wake_owner(tmv_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *snap = tmv_wake_owner_snapshot(ctx, NULL);
    if (tmv_is_raise(snap)) return snap;
    json_t *owner = json_get(snap, 0);
    if (!owner || owner->type == JSON_NULL) { json_free(snap); return json_bool(false); }
    json_t *a = tmv_args0();
    tmv_aset(a, "transport", owner);
    json_t *r = tmv_release_wake_for_transport(ctx, a);
    json_free(snap);
    return r;
}

/* PoP: _persist_wake_enabled @ tui_gateway/methods_voice.py:_persist_wake_enabled */
static json_t *tmv_persist_wake_enabled(tmv_ctx_t *ctx, const json_t *args) {
    bool enabled = json_get_bool(args, "enabled", false);
    const char *const NAMES[] = {"save_config_value"};
    json_t *imp = tmv_import(ctx, "cli", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (tmv_caught_by_exception(imp)) { json_free(imp); return json_bool(false); }
        return imp;
    }
    json_free(imp);
    json_t *a = tmv_args0();
    tmv_asets(a, "key", "wake_word.enabled");
    tmv_asetb(a, "value", enabled);
    json_t *r = tmv_call(ctx, "save_config_value", a);
    if (tmv_is_raise(r)) {
        if (tmv_caught_by_exception(r)) { json_free(r); return json_bool(false); }
        return r;
    }
    bool hit = tmv_truthy(r);
    json_free(r);
    return json_bool(hit);
}

/* PoP: _owner_result @ tui_gateway/methods_voice.py:_owner_result */
static json_t *tmv_owner_result(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *field = json_obj_get(args, "field");
    const char *fs = tmv_str(field) ? tmv_str(field) : "";
    bool ok = json_get_bool(args, "ok", false);
    json_t *result = json_object();
    json_set(result, fs, json_bool(ok));
    json_set(result, "reason", ok ? json_null() : json_string("not_owner"));
    const json_t *extra = json_obj_get(args, "extra");
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(result, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    return tmv_ok(ctx, json_obj_get(args, "rid"), result);
}

/* PoP: _deliver_fd_transcript @ tui_gateway/methods_voice.py:_deliver_fd_transcript */
static json_t *tmv_deliver_fd_transcript(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    const char *tx = tmv_str(text) ? tmv_str(text) : "";
    const char *const NAMES[] = {"is_voice_stop_phrase"};
    bool is_stop = false;
    json_t *imp = tmv_import(ctx, "tools.voice_mode_transcript", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (tmv_caught_by_exception(imp)) json_free(imp);
        else return imp;
    } else {
        json_free(imp);
        json_t *a = tmv_args0();
        tmv_aset(a, "text", text);
        json_t *r = tmv_call(ctx, "is_voice_stop_phrase", a);
        if (tmv_is_raise(r)) {
            if (tmv_caught_by_exception(r)) json_free(r);
            else return r;
        } else { is_stop = tmv_truthy(r); json_free(r); }
    }
    if (is_stop) {
        const char *sid = tmv_state_str(ctx, "voice_event_sid", "");
        json_t *a = tmv_args0();
        tmv_asets(a, "session_id", sid);
        tmv_asetb(a, "stop_loop", true);
        tmv_asetb(a, "stop_tts", false);
        json_t *r = tmv_end_session_voice_chat(ctx, a);
        if (tmv_is_raise(r)) return r;
        json_free(r);
    }
    json_t *payload = json_object();
    if (is_stop) {
        json_set(payload, "stop_phrase", json_bool(true));
        json_set(payload, "text", json_copy(text));
    } else {
        json_set(payload, "text", json_copy(text));
    }
    json_t *e = tmv_emit_call(ctx, "voice.transcript",
                              tmv_state_str(ctx, "voice_event_sid", ""), payload);
    if (tmv_is_raise(e)) return e;
    json_free(e);
    (void)tx;
    return json_null();
}

/* PoP: _voice_toggle_status @ tui_gateway/methods_voice.py:_voice_toggle_status */
static json_t *tmv_voice_toggle_status(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *payload = tmv_voice_status_payload(ctx, tmv_args0());
    if (tmv_is_raise(payload)) return payload;
    const char *const NAMES[] = {"check_voice_requirements"};
    json_t *imp = tmv_import(ctx, "tools.voice_mode", NAMES, 1);
    if (tmv_is_raise(imp)) {
        if (tmv_caught_by_exception(imp)) json_free(imp);
        else { json_free(payload); return imp; }
    } else {
        json_free(imp);
        json_t *r = tmv_call(ctx, "check_voice_requirements", tmv_args0());
        if (tmv_is_raise(r)) {
            if (tmv_caught_by_exception(r)) json_free(r);
            else { json_free(payload); return r; }
        } else {
            if (r && r->type == JSON_OBJECT) {
                const char *keys[] = {"available", "audio_available", "stt_available"};
                for (int i = 0; i < 3; i++)
                    json_set(payload, keys[i], json_bool(tmv_truthy(json_obj_get(r, keys[i]))));
                const json_t *det = json_obj_get(r, "details");
                json_set(payload, "details", (det && tmv_truthy(det)) ? json_copy(det) : json_string(""));
            }
            json_free(r);
        }
    }
    return tmv_ok(ctx, rid, payload);
}

/* PoP: _set_voice_tts @ tui_gateway/methods_voice.py:_set_voice_tts */
static json_t *tmv_set_voice_tts(tmv_ctx_t *ctx, const json_t *args) {
    bool on = json_get_bool(args, "on", false);
    json_t *r = tmv_env_set(ctx, "HERMES_VOICE_TTS", on ? "1" : "0");
    if (tmv_is_raise(r)) return r;
    json_free(r);
    if (!on) {
        json_t *a = tmv_args0();
        tmv_asetb(a, "user_barge", false);
        json_t *st = tmv_call(ctx, "tts_stream_stop", a);
        if (tmv_is_raise(st)) return st;
        json_free(st);
    }
    json_t *a = tmv_args0();
    tmv_asets(a, "lease", "tui:voice-tts");
    tmv_asetb(a, "active", on);
    json_t *lr = tmv_call(ctx, "tts_lease_async", a);
    if (tmv_is_raise(lr)) return lr;
    json_free(lr);
    return json_null();
}

/* PoP: _voice_toggle_tts @ tui_gateway/methods_voice.py:_voice_toggle_tts */
static json_t *tmv_voice_toggle_tts(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *v = tmv_voice_mode_enabled(ctx, NULL);
    if (tmv_is_raise(v)) return v;
    bool on = tmv_truthy(v);
    json_free(v);
    if (!on) return tmv_err(ctx, rid, 4014, "enable voice mode first: /voice on");
    json_t *tt = tmv_voice_tts_enabled(ctx, NULL);
    if (tmv_is_raise(tt)) return tt;
    bool newon = !tmv_truthy(tt);
    json_free(tt);
    json_t *a = tmv_args0();
    tmv_asetb(a, "on", newon);
    json_t *r = tmv_set_voice_tts(ctx, a);
    if (tmv_is_raise(r)) return r;
    json_free(r);
    json_t *payload = tmv_voice_status_payload(ctx, tmv_args0());
    if (tmv_is_raise(payload)) return payload;
    return tmv_ok(ctx, rid, payload);
}

/* PoP: _vr_transcript @ tui_gateway/methods_voice.py:_vr_transcript */
static json_t *tmv_vr_transcript(tmv_ctx_t *ctx, const json_t *args) {
    const json_t *payload = json_obj_get(args, "payload");
    json_t *r = tmv_emit_call(ctx, "voice.transcript",
                              tmv_state_str(ctx, "voice_event_sid", ""), payload);
    if (tmv_is_raise(r)) return r;
    json_free(r);
    json_t *r2 = tmv_resume_voice_wake(ctx, NULL);
    if (tmv_is_raise(r2)) return r2;
    json_free(r2);
    return json_null();
}
