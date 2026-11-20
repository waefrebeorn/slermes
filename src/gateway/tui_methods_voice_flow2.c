/*
 * tui_methods_voice_flow2.c -- FLOW2 port of the REMAINING unclaimed voice
 * method surface of tui_gateway/methods_voice.py (the flow1 oracle
 * tui_methods_voice_flow claimed 27 functions; this file claims a further 9
 * unclaimed top-level defs). Every PoP below credits the REAL defining module.
 *
 * Ports the DECISION SEQUENCES -- branch order, seam call order, error-code
 * composition, import/except domains and state mutation -- not just leaf
 * outputs. Every live external call enters through ONE universal seam:
 * tmv2_call(ctx, name, args). Claimed flow1 siblings are SEAMED (voice_emit,
 * resume_voice_wake, set_session_voice_off, vr_transcript,
 * end_session_voice_chat, voice_tts_enabled, tts_lease_async, set_voice_tts,
 * voice_status_payload); ported siblings call each other in-line so their own
 * seam events appear in live evaluation order.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmv2_-prefixed.
 *
 * Representation contract (shared with the oracle):
 * - None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
 *   BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except-Exception site.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - integral floats render as ints; str()/strip() fold the ASCII fixture corpus.
 *
 * Ownership: tmv2_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value. Seam results are caller-freed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct tmv2_ctx {
    json_t *state;      /* owned; module-global mirror, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmv2_ctx_t;

/* -- basic helpers -- */
static bool tmv2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *tmv2_raise_kind(const json_t *v) {
    return tmv2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool tmv2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static json_t *tmv2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* -- full builtin exception hierarchy (py3.14.9) -- */
typedef struct { const char *name; const char *parent; } tmv2_exc_row_t;
static const tmv2_exc_row_t TMV2_EXC_PARENTS[] = {
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
#define TMV2_EXC_PARENT_COUNT (sizeof(TMV2_EXC_PARENTS)/sizeof(TMV2_EXC_PARENTS[0]))
static const char *tmv2_exc_parent(const char *kind) {
    if (!kind) return NULL;
    for (size_t i = 0; i < TMV2_EXC_PARENT_COUNT; i++)
        if (!strcmp(TMV2_EXC_PARENTS[i].name, kind)) return TMV2_EXC_PARENTS[i].parent;
    return NULL;
}
/* issubclass(kind, target) over the single-parent table. Unknown kinds are
 * synthetic Exception subclasses (the oracle's make_exc fallback). */
static bool tmv2_exc_is_a(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (!strcmp(cur, target)) return true;
        if (!strcmp(cur, "BaseException") || !strcmp(cur, "object")) return false;
        const char *p = tmv2_exc_parent(cur);
        if (!p) return !strcmp(target, "Exception");
        cur = p;
    }
    return false;
}
/* except Exception / suppress(Exception): swallow Exception subclasses;
 * BaseException-only kinds pass through. */
static bool tmv2_caught_by_exception(const json_t *r) {
    return tmv2_exc_is_a(tmv2_raise_kind(r), "Exception");
}
/* Models `with suppress(Exception): ...` / `try: ... except Exception`:
 * returns 0 = clean (proceed), 1 = swallowed (skip rest of block),
 * -1 = must propagate (the owned raise node is written to *out). */
static int tmv2_guard(json_t *r, json_t **out) {
    if (!tmv2_is_raise(r)) { json_free(r); return 0; }
    if (tmv2_caught_by_exception(r)) { json_free(r); return 1; }
    *out = r; return -1;
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tmv2_call(tmv2_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tmv2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmv2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT)
                ret = tmv2_err_spec(json_get_str(raise, "type", "RuntimeError"),
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
static json_t *tmv2_args0(void) { return json_object(); }
static void tmv2_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmv2_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmv2_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }

/* -- state helpers -- */
static json_t *tmv2_state_get(tmv2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
static const char *tmv2_state_str(tmv2_ctx_t *ctx, const char *key, const char *def) {
    const json_t *v = tmv2_state_get(ctx, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static void tmv2_state_set_bool(tmv2_ctx_t *ctx, const char *key, bool val) {
    json_set(ctx->state, key, json_bool(val));
}

/* -- seam wrappers -- */
static json_t *tmv2_import(tmv2_ctx_t *ctx, const char *module,
                           const char *const *names, size_t n) {
    json_t *a = tmv2_args0();
    tmv2_asets(a, "module", module);
    json_t *fl = json_array();
    for (size_t i = 0; i < n; i++) json_append(fl, json_string(names[i] ? names[i] : ""));
    json_set(a, "fromlist", fl);
    return tmv2_call(ctx, "import", a);
}

/* -- forward declarations of ported functions -- */
static json_t *tmv2_frame_fields(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_register(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_vr_on_status(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_vr_on_silent_limit(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_vr_on_stop_phrase(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_stop_session_voice_capture(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_arm_full_duplex_listener(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_wake_probe(tmv2_ctx_t *, const json_t *);
static json_t *tmv2_voice_toggle_mode(tmv2_ctx_t *, const json_t *);

/* ========================================================================
 * PORTED FUNCTIONS -- one PoP annotation each
 * ======================================================================== */

/* PoP: tmv2_frame_fields @ tui_gateway/methods_voice.py:_frame_fields */
static json_t *tmv2_frame_fields(tmv2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *frame = json_obj_get(args, "frame");
    json_t *out = json_object();
    const char *keys[2] = {"sample_rate", "frame_length"};
    double defs[2] = {16000.0, 1280.0};
    for (int i = 0; i < 2; i++) {
        const json_t *v = (frame && frame->type == JSON_OBJECT)
                            ? json_obj_get(frame, keys[i]) : NULL;
        if (v) json_set(out, keys[i], json_copy(v));
        else   json_set(out, keys[i], json_number(defs[i]));
    }
    return out;
}

/* PoP: tmv2_register @ tui_gateway/methods_voice.py:register */
static json_t *tmv2_register(tmv2_ctx_t *ctx, const json_t *args) {
    /* bind_module(globals(), server, skip=("_",)) is a bare expression
     * statement -> returns None. Propagate any scripted raise (no except). */
    json_t *a = tmv2_args0();
    tmv2_aset(a, "server", json_obj_get(args, "server"));
    json_t *skip = json_array();
    json_append(skip, json_string("_"));
    json_set(a, "skip", skip);
    json_t *r = tmv2_call(ctx, "bind_module", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);   /* None regardless of seam result */
    return json_null();
}

/* PoP: tmv2_vr_on_status @ tui_gateway/methods_voice.py:_vr_on_status */
static json_t *tmv2_vr_on_status(tmv2_ctx_t *ctx, const json_t *args) {
    const json_t *state = json_obj_get(args, "state");
    /* _voice_emit("voice.status", {"state": state}) -- claimed flow1 sibling */
    json_t *a = tmv2_args0();
    tmv2_asets(a, "event", "voice.status");
    json_t *payload = json_object();
    tmv2_aset(payload, "state", state);
    json_set(a, "payload", payload);
    json_t *r = tmv2_call(ctx, "voice_emit", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    if (state && state->type == JSON_STRING && state->str_val &&
        !strcmp(state->str_val, "idle")) {
        r = tmv2_call(ctx, "resume_voice_wake", tmv2_args0());
        if (tmv2_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: tmv2_vr_on_silent_limit @ tui_gateway/methods_voice.py:_vr_on_silent_limit */
static json_t *tmv2_vr_on_silent_limit(tmv2_ctx_t *ctx, const json_t *args) {
    (void)args;
    /* _set_session_voice_off(_voice_capture_sid) */
    json_t *a = tmv2_args0();
    tmv2_asets(a, "session_id", tmv2_state_str(ctx, "voice_capture_sid", ""));
    json_t *r = tmv2_call(ctx, "set_session_voice_off", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    /* _vr_transcript({"no_speech_limit": True}) */
    a = tmv2_args0();
    json_t *payload = json_object();
    json_set(payload, "no_speech_limit", json_bool(true));
    json_set(a, "payload", payload);
    r = tmv2_call(ctx, "vr_transcript", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tmv2_vr_on_stop_phrase @ tui_gateway/methods_voice.py:_vr_on_stop_phrase */
static json_t *tmv2_vr_on_stop_phrase(tmv2_ctx_t *ctx, const json_t *args) {
    /* _end_session_voice_chat(_voice_capture_sid, stop_loop=False, stop_tts=True) */
    json_t *a = tmv2_args0();
    tmv2_asets(a, "session_id", tmv2_state_str(ctx, "voice_capture_sid", ""));
    tmv2_asetb(a, "stop_loop", false);
    tmv2_asetb(a, "stop_tts", true);
    json_t *r = tmv2_call(ctx, "end_session_voice_chat", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    /* _vr_transcript({"stop_phrase": True, "text": t}) */
    a = tmv2_args0();
    json_t *payload = json_object();
    json_set(payload, "stop_phrase", json_bool(true));
    tmv2_aset(payload, "text", json_obj_get(args, "t"));
    json_set(a, "payload", payload);
    r = tmv2_call(ctx, "vr_transcript", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tmv2_stop_session_voice_capture @ tui_gateway/methods_voice.py:_stop_session_voice_capture */
static json_t *tmv2_stop_session_voice_capture(tmv2_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    const char *cur = tmv2_state_str(ctx, "voice_capture_sid", "");
    if (!sid[0] || strcmp(sid, cur) != 0) return json_null();
    /* _voice_capture_gen += 1 */
    json_t *gen = tmv2_state_get(ctx, "voice_capture_gen");
    double gv = (gen && gen->type == JSON_NUMBER) ? gen->num_val : 0.0;
    json_set(ctx->state, "voice_capture_gen", json_number(gv + 1.0));
    /* with contextlib.suppress(Exception): from ... import stop_continuous; stop_continuous() */
    static const char *const names[] = {"stop_continuous"};
    json_t *prop = NULL;
    int g = tmv2_guard(tmv2_import(ctx, "hermes_cli.voice", names, 1), &prop);
    if (g < 0) return prop;
    if (g == 0) {
        g = tmv2_guard(tmv2_call(ctx, "stop_continuous", tmv2_args0()), &prop);
        if (g < 0) return prop;
    }
    /* _resume_voice_wake() -- no except here */
    json_t *r = tmv2_call(ctx, "resume_voice_wake", tmv2_args0());
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tmv2_arm_full_duplex_listener @ tui_gateway/methods_voice.py:_arm_full_duplex_listener */
static json_t *tmv2_arm_full_duplex_listener(tmv2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *active = tmv2_state_get(ctx, "fd_listener_active");
    if (active && active->type == JSON_BOOL && active->bool_val) return json_null();
    tmv2_state_set_bool(ctx, "fd_listener_active", true);
    /* threading.Thread(target=_full_duplex_listener, daemon=True,
     * name="voice-full-duplex").start() */
    json_t *a = tmv2_args0();
    tmv2_asets(a, "target", "_full_duplex_listener");
    tmv2_asetb(a, "daemon", true);
    tmv2_asets(a, "name", "voice-full-duplex");
    json_t *r = tmv2_call(ctx, "thread_start", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: tmv2_wake_probe @ tui_gateway/methods_voice.py:_wake_probe */
static json_t *tmv2_wake_probe(tmv2_ctx_t *ctx, const json_t *args) {
    static const char *const names[] = {
        "check_wake_word_requirements", "resolve_capture_mode"
    };
    json_t *r = tmv2_import(ctx, "tools.wake_word", names, 2);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    const json_t *cfg = json_obj_get(args, "cfg");
    const json_t *params = json_obj_get(args, "params");
    const char *surface = json_get_str(args, "surface", "");
    /* prefer_client = surface in ("gui","desktop") or bool(params.get("client_capture")) */
    bool prefer = false;
    if (surface && (!strcmp(surface, "gui") || !strcmp(surface, "desktop"))) prefer = true;
    if (!prefer && params && params->type == JSON_OBJECT)
        prefer = tmv2_truthy(json_obj_get(params, "client_capture"));
    /* resolve_capture_mode(cfg, prefer_client=prefer) */
    json_t *a = tmv2_args0();
    tmv2_aset(a, "cfg", cfg);
    tmv2_asetb(a, "prefer_client", prefer);
    json_t *capture = tmv2_call(ctx, "resolve_capture_mode", a);
    if (tmv2_is_raise(capture)) return capture;
    /* check_wake_word_requirements({**cfg, "capture": capture_mode}) */
    json_t *cfg2 = json_object();
    if (cfg && cfg->type == JSON_OBJECT) {
        for (size_t i = 0; i < cfg->c.count; i++)
            json_set(cfg2, cfg->c.keys[i], json_copy(cfg->c.items[i]));
    }
    json_set(cfg2, "capture", json_copy(capture));
    a = tmv2_args0();
    json_set(a, "cfg", cfg2);
    json_t *reqs = tmv2_call(ctx, "check_wake_word_requirements", a);
    if (tmv2_is_raise(reqs)) { json_free(capture); return reqs; }
    /* tuple -> list */
    json_t *out = json_array();
    json_append(out, capture);
    json_append(out, reqs);
    return out;
}

/* PoP: tmv2_voice_toggle_mode @ tui_gateway/methods_voice.py:_voice_toggle_mode */
static json_t *tmv2_voice_toggle_mode(tmv2_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    const char *action = (params && params->type == JSON_OBJECT)
                            ? json_get_str(params, "action", "") : "";
    bool enabled = (action && !strcmp(action, "on"));
    /* os.environ["HERMES_VOICE"] = "1" if enabled else "0" */
    json_t *a = tmv2_args0();
    tmv2_asets(a, "key", "HERMES_VOICE");
    tmv2_asets(a, "value", enabled ? "1" : "0");
    json_t *r = tmv2_call(ctx, "env_set", a);
    if (tmv2_is_raise(r)) return r;
    json_free(r);
    char stop_hint[512] = "";
    json_t *prop = NULL;
    if (enabled) {
        /* with contextlib.suppress(Exception): import voice_stop_hint; stop_hint = voice_stop_hint() */
        static const char *const names[] = {"voice_stop_hint"};
        int g = tmv2_guard(tmv2_import(ctx, "tools.voice_mode_transcript", names, 1), &prop);
        if (g < 0) return prop;
        if (g == 0) {
            r = tmv2_call(ctx, "voice_stop_hint", tmv2_args0());
            if (tmv2_is_raise(r)) {
                g = tmv2_guard(r, &prop);
                if (g < 0) return prop;
            } else {
                const char *s = (r->type == JSON_STRING) ? r->str_val : NULL;
                snprintf(stop_hint, sizeof stop_hint, "%s", s ? s : "");
                json_free(r);
            }
        }
        /* if _voice_tts_enabled(): _tts_lease_async("tui:voice-tts", True) */
        r = tmv2_call(ctx, "voice_tts_enabled", tmv2_args0());
        if (tmv2_is_raise(r)) return r;
        bool tts_on = tmv2_truthy(r);
        json_free(r);
        if (tts_on) {
            a = tmv2_args0();
            tmv2_asets(a, "lease", "tui:voice-tts");
            tmv2_asetb(a, "active", true);
            r = tmv2_call(ctx, "tts_lease_async", a);
            if (tmv2_is_raise(r)) return r;
            json_free(r);
        }
    } else {
        /* try: from hermes_cli.voice import stop_continuous; stop_continuous()
         * except ImportError: pass / except Exception: logger.warning(...) -- both silent. */
        static const char *const names[] = {"stop_continuous"};
        int g = tmv2_guard(tmv2_import(ctx, "hermes_cli.voice", names, 1), &prop);
        if (g < 0) return prop;
        if (g == 0) {
            g = tmv2_guard(tmv2_call(ctx, "stop_continuous", tmv2_args0()), &prop);
            if (g < 0) return prop;
        }
        /* _set_voice_tts(False) */
        a = tmv2_args0();
        tmv2_asetb(a, "on", false);
        r = tmv2_call(ctx, "set_voice_tts", a);
        if (tmv2_is_raise(r)) return r;
        json_free(r);
    }
    /* return _ok(rid, _voice_status_payload(stop_hint=stop_hint)) */
    a = tmv2_args0();
    tmv2_asets(a, "stop_hint", stop_hint);
    json_t *payload = tmv2_call(ctx, "voice_status_payload", a);
    if (tmv2_is_raise(payload)) return payload;
    a = tmv2_args0();
    tmv2_aset(a, "rid", json_obj_get(args, "rid"));
    json_set(a, "result", payload);
    json_t *ret = tmv2_call(ctx, "ok", a);
    return ret;
}
