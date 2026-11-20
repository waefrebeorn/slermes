/*
 * run_busy_flow2.c — remainder FLOW cores of gateway/run_busy.py that the
 * flow1 port (run_busy_flow.c) did NOT claim. 12 UNCLAIMED defs are ported:
 *
 *   approval_input_words, _tail_has_slot, _destructive_optout_note,
 *   _busy_reply_to, _demote_interrupt, _command_handler_table,
 *   _gateway_idle_command_handlers, _plaintext_approval_words,
 *   _get_max_concurrent_sessions, _active_session_limit_message,
 *   _route_stop_epoch, _send_busy_ack_reply.
 *
 * Modeling contract (shared with tests/oracle_run_busy_flow2.py):
 *  - Every live collaborator enters through ONE universal seam; the seam
 *    script is injected as in["io"] (a name -> result or per-call queue).
 *  - A seam result is NULL == Python None; {"__raise__":kind,"message":m}
 *    == a raised exception. Scripts use the SAME caller-recorded {ev,args,ret}
 *    trace the oracle builds with recording stubs on the live Python.
 *  - Full builtin exception hierarchy: an `except Exception` site folds ONLY
 *    Exception subclasses (walk the parent chain); BaseException-only kinds
 *    (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError, and
 *    BaseException itself) propagate via env->exc.
 *  - Pure string ops (strip/lower/whole-slot) mirror Python semantics used by
 *    these functions; fixture t()/text values are ASCII so lower is exact.
 *  - Self-contained: the harness #includes this .c; every symbol is static.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gateway_lifecycle_flow.h"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct rb2_env {
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise spec; NULL when none */
} rb2_env;

/* Universal seam call: args BORROWED by the stub (it copies into the trace),
 * result NEW (caller frees). NULL result == Python None. */
static json_t *rb2_call(rb2_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

/* ── full builtin exception hierarchy ─────────────────────────────────────── */
static bool rb2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && gflow_is_str(json_obj_get(r, "__raise__"));
}
static const char *rb2_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *rb2_msg(const json_t *r) {
    return json_get_str(r, "message", "");
}
static bool rb2_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}
/* Immediate parent in the Python builtin exception hierarchy (NULL = root). */
static const char *rb2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"CancelledError", "BaseException"},
        {"ArithmeticError", "Exception"}, {"BufferError", "Exception"},
        {"EOFError", "Exception"}, {"ImportError", "Exception"},
        {"LookupError", "Exception"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"OSError", "Exception"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"StopIteration", "Exception"},
        {"SyntaxError", "Exception"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"Warning", "Exception"}, {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"FloatingPointError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"}, {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"}, {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"}, {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"ModuleNotFoundError", "ImportError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"UnboundLocalError", "NameError"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"UnicodeError", "ValueError"}, {"JSONDecodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"}, {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        {"BytesWarning", "Warning"}, {"DeprecationWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"}, {"ResourceWarning", "Warning"},
        {"RuntimeWarning", "Warning"}, {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (rb2_eq(T[i].c, c)) return T[i].p;
    return NULL;   /* unknown custom kind: treat as a leaf (not Exception) */
}
/* Does `cls` (or any ancestor) match one of `classes`? */
static bool rb2_exc_matches(const char *cls, const char *const *classes, size_t n) {
    for (int depth = 0; depth < 64 && cls; depth++) {
        for (size_t i = 0; i < n; i++) if (rb2_eq(cls, classes[i])) return true;
        cls = rb2_exc_parent(cls);
    }
    return false;
}
/* Matches `except Exception` (NOT the BaseException-only branches). */
static bool rb2_exc_is_exception(const char *cls) {
    static const char *E[] = {"Exception"};
    return rb2_exc_matches(cls, E, 1);
}

/* ── unicode string helpers (mirror the flow1 port) ───────────────────────── */
static unsigned long rb2_cp(const char *s, size_t *adv) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned long cp; size_t n;
    if (u[0] < 0x80) { cp = u[0]; n = 1; }
    else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; n = 4; }
    else { cp = u[0]; n = 1; }
    for (size_t i = 1; i < n && u[i]; i++) cp = (cp << 6) | (u[i] & 0x3F);
    *adv = n;
    return cp;
}
static bool rb2_ws(unsigned long cp) {
    return (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || (cp >= 0x1C && cp <= 0x1F)
        || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}
/* Python str.strip() over the Unicode White_Space set; malloc'd. */
static char *rb2_strip(const char *s) {
    if (!s) return strdup("");
    size_t start = 0, last = 0, i = 0; bool any = false;
    while (s[i]) {
        size_t adv; unsigned long cp = rb2_cp(s + i, &adv);
        if (!rb2_ws(cp)) { if (!any) { start = i; any = true; } last = i + adv; }
        i += adv;
    }
    if (!any) return strdup("");
    char *r = (char *)malloc(last - start + 1);
    if (!r) return strdup("");
    memcpy(r, s + start, last - start);
    r[last - start] = '\0';
    return r;
}
/* ASCII str.lower(); fixtures use ASCII word text so this is exact. */
static char *rb2_lower(const char *s) {
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return r;
}
/* Whole-slot remainder (Python _strip_slot): "" for exact, NULL otherwise. */
static char *rb2_strip_slot_str(const char *text, const char *slot) {
    if (!text || !slot) return NULL;
    if (strcmp(text, slot) == 0) return strdup("");
    size_t sl = strlen(slot);
    if (strncmp(text, slot, sl) == 0 && text[sl] == ':')
        return strdup(text + sl + 1);
    return NULL;
}

/* ── log seam (raw fmt + args; stub emits {ev:"log",level,fmt,args}) ──────── */
static void rb2_log(rb2_env *e, int level, const char *fmt, json_t *args) {
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args);   /* adopts */
    json_free(rb2_call(e, "log", a));
}
static json_t *rb2_logargs1(const char *s) {
    json_t *a = json_array();
    json_append(a, json_string(s ? s : ""));
    return a;
}

/* ── approval_input_words internals ───────────────────────────────────────── */
/* The live function calls t(key, lang=DEFAULT_LANGUAGE) then t(key); both are
 * one "t" seam queue. DEFAULT_LANGUAGE is seeded as "en" on both sides. */
static json_t *rb2_approval_words_key(rb2_env *e, const char *input_key) {
    char key[512];
    snprintf(key, sizeof(key), "approval.inputs.%s", input_key ? input_key : "");
    json_t *out = json_array();
    for (int call = 0; call < 2; call++) {
        json_t *a = json_object();
        json_set(a, "key", json_string(key));
        if (call == 0) json_set(a, "lang", json_string("en"));
        json_t *r = rb2_call(e, "t", a);
        if (rb2_is_raise(r)) { e->exc = r; json_free(out); return NULL; }
        const char *raw = gflow_str(r);
        if (raw && strcmp(raw, key) == 0) { json_free(r); continue; }  /* catalog miss */
        if (raw) {
            const char *p = raw;
            for (;;) {
                const char *comma = strchr(p, ',');
                size_t n = comma ? (size_t)(comma - p) : strlen(p);
                char *seg = (char *)malloc(n + 1);
                if (seg) {
                    memcpy(seg, p, n); seg[n] = '\0';
                    char *w = rb2_strip(seg);
                    char *lw = rb2_lower(w);
                    if (lw[0]) {
                        bool seen = false;
                        for (size_t i = 0; i < json_len(out); i++) {
                            const char *x = gflow_str(json_get(out, i));
                            if (x && strcmp(x, lw) == 0) { seen = true; break; }
                        }
                        if (!seen) json_append(out, json_string(lw));
                    }
                    free(lw); free(w); free(seg);
                }
                if (!comma) break;
                p = comma + 1;
            }
        }
        json_free(r);
    }
    return out;
}

/* PoP: rb2_approval_input_words @ gateway/run_busy.py:approval_input_words */
static json_t *rb2_approval_input_words(rb2_env *e, const json_t *in) {
    return rb2_approval_words_key(e, json_get_str(in, "input_key", ""));
}

/* PoP: rb2_tail_has_slot @ gateway/run_busy.py:_tail_has_slot */
static json_t *rb2_tail_has_slot(rb2_env *e, const json_t *in) {
    (void)e;
    char *r = rb2_strip_slot_str(json_get_str(in, "tail", ""), json_get_str(in, "slot", ""));
    bool ok = (r != NULL);
    free(r);
    return json_bool(ok);
}

/* PoP: rb2_destructive_optout_note @ gateway/run_busy.py:_destructive_optout_note */
static json_t *rb2_destructive_optout_note(rb2_env *e, const json_t *in) {
    bool persisted = gflow_truth(json_obj_get(in, "persisted"));
    json_t *a = json_object();
    json_set(a, "key", json_string(persisted
        ? "gateway.confirm.destructive_optout"
        : "gateway.confirm.destructive_optout_save_failed"));
    json_t *r = rb2_call(e, "t", a);
    if (rb2_is_raise(r)) { e->exc = r; return NULL; }
    return r;   /* the localized note string */
}

/* PoP: rb2_busy_reply_to @ gateway/run_busy.py:_busy_reply_to */
static json_t *rb2_busy_reply_to(rb2_env *e, const json_t *in) {
    (void)e;
    const json_t *ev = json_obj_get(in, "event");
    const json_t *src = json_obj_get(ev, "source");
    bool is_tg = rb2_eq(json_get_str(src, "platform", ""), "telegram");
    bool thread = gflow_truth(json_obj_get(src, "thread_id"));
    if (is_tg && thread) {
        bool dm = rb2_eq(json_get_str(src, "chat_type", ""), "dm");
        const json_t *anchor = json_obj_get(in, "reply_anchor");
        if (dm) return anchor ? json_copy(anchor) : json_null();
        return json_null();
    }
    if (json_get_bool(ev, "internal", false)) return json_null();
    const json_t *mid = json_obj_get(ev, "message_id");
    return mid ? json_copy(mid) : json_null();
}

/* PoP: rb2_demote_interrupt @ gateway/run_busy.py:_demote_interrupt */
static json_t *rb2_demote_interrupt(rb2_env *e, const json_t *in) {
    json_t *wa = json_array();
    json_append(wa, json_string(json_get_str(in, "session_key", "")));
    json_append(wa, json_string(json_get_str(in, "why", "")));
    rb2_log(e, 20, "Demoting busy_input_mode 'interrupt' to 'queue' for session %s because %s", wa);
    return json_string("queue");
}

/* _handle_<name.replace('-','_')>_command, built without snprintf-into-small. */
static char *rb2_handler_name(const char *name) {
    size_t n = strlen(name ? name : "");
    char *rep = (char *)malloc(n + 1);
    if (!rep) return strdup("");
    for (size_t i = 0; i < n; i++) rep[i] = name[i] == '-' ? '_' : name[i];
    rep[n] = '\0';
    char *r = (char *)malloc(n + sizeof("_handle__command"));
    if (r) snprintf(r, n + sizeof("_handle__command"), "_handle_%s_command", rep);
    free(rep);
    return r ? r : strdup("");
}
/* _command_handler_table core: names -> getattr(self, alias or derived). */
static json_t *rb2_handler_table(rb2_env *e, const json_t *names, const json_t *aliases) {
    json_t *out = json_object();
    if (!names || names->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(names); i++) {
        const char *name = gflow_str(json_get(names, i));
        if (!name) continue;
        const char *alias = json_get_str(aliases, name, NULL);
        char *derived = NULL;
        const char *hn = (alias && alias[0]) ? alias : (derived = rb2_handler_name(name));
        json_t *a = json_object();
        json_set(a, "handler_name", json_string(hn));
        json_t *r = rb2_call(e, "getattr_handler", a);
        if (rb2_is_raise(r)) { e->exc = r; json_free(out); free(derived); return NULL; }
        json_set(out, name, r ? r : json_null());   /* adopts r */
        free(derived);
    }
    return out;
}

/* PoP: rb2_command_handler_table @ gateway/run_busy.py:_command_handler_table */
static json_t *rb2_command_handler_table(rb2_env *e, const json_t *in) {
    return rb2_handler_table(e, json_obj_get(in, "names"), json_obj_get(in, "aliases"));
}

/* PoP: rb2_gateway_idle_command_handlers @ gateway/run_busy.py:_gateway_idle_command_handlers */
static json_t *rb2_gateway_idle_handlers(rb2_env *e, const json_t *in) {
    return rb2_handler_table(e, json_obj_get(in, "idle_commands"), json_obj_get(in, "aliases"));
}

/* PoP: rb2_plaintext_approval_words @ gateway/run_busy.py:_plaintext_approval_words */
static json_t *rb2_plaintext_approval_words(rb2_env *e, const json_t *in) {
    (void)in;
    json_t *out = json_object();
    /* dict(self._PLAINTEXT_APPROVAL_EXTRA_WORDS) — 👍 / 👎 first. */
    {
        json_t *v = json_array(); json_append(v, json_string("approve")); json_append(v, json_string(""));
        json_set(out, "\xf0\x9f\x91\x8d", v);
        v = json_array(); json_append(v, json_string("deny")); json_append(v, json_string(""));
        json_set(out, "\xf0\x9f\x91\x8e", v);
    }
    static const char *keys[] = {"approve", "deny", "always", "session"};
    static const char *verbs[] = {"approve", "deny", "approve", "approve"};
    static const char *args4[] = {"", "", "always", "session"};
    for (int i = 0; i < 4; i++) {
        json_t *words = rb2_approval_words_key(e, keys[i]);
        if (e->exc) { json_free(words); json_free(out); return NULL; }
        for (size_t j = 0; j < json_len(words); j++) {
            const char *w = gflow_str(json_get(words, j));
            if (!w || json_obj_get(out, w)) continue;   /* setdefault */
            json_t *va = json_array();
            json_append(va, json_string(verbs[i]));
            json_append(va, json_string(args4[i]));
            json_set(out, w, va);
        }
        json_free(words);
    }
    return out;
}

/* PoP: rb2_get_max_concurrent_sessions @ gateway/run_busy.py:_get_max_concurrent_sessions */
static json_t *rb2_get_max_concurrent_sessions(rb2_env *e, const json_t *in) {
    json_t *a = json_object();
    const json_t *cfg = json_obj_get(in, "config");
    json_set(a, "config", cfg ? json_copy(cfg) : json_null());
    json_t *r = rb2_call(e, "resolve_max_concurrent_sessions", a);
    if (rb2_is_raise(r)) {
        if (rb2_exc_is_exception(rb2_kind(r))) { json_free(r); return json_null(); }
        e->exc = r; return NULL;   /* BaseException-only propagates */
    }
    return r;
}

/* PoP: rb2_active_session_limit_message @ gateway/run_busy.py:_active_session_limit_message */
static json_t *rb2_active_session_limit_message(rb2_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    json_t *mx = rb2_get_max_concurrent_sessions(e, in);
    if (e->exc) return NULL;
    if (gflow_is_null(mx)) { json_free(mx); return json_null(); }
    json_t *a = json_object();
    json_set(a, "session_key", json_string(key));
    json_t *isr = rb2_call(e, "is_session_running", a);
    if (rb2_is_raise(isr)) { e->exc = isr; json_free(mx); return NULL; }
    bool running = gflow_truth(isr);
    json_free(isr);
    if (running) { json_free(mx); return json_null(); }
    json_t *acr = rb2_call(e, "running_agent_count", json_object());
    if (rb2_is_raise(acr)) { e->exc = acr; json_free(mx); return NULL; }
    double ac = (acr && acr->type == JSON_NUMBER) ? acr->num_val : 0.0;
    json_free(acr);
    double maxv = (mx && mx->type == JSON_NUMBER) ? mx->num_val : 0.0;
    if (ac < maxv) { json_free(mx); return json_null(); }
    json_t *la = json_object();
    json_set(la, "active_count", json_number(ac));
    json_set(la, "max_sessions", json_number(maxv));
    json_free(mx);
    json_t *res = rb2_call(e, "active_session_limit_message", la);
    if (rb2_is_raise(res)) { e->exc = res; return NULL; }
    return res;
}

/* PoP: rb2_route_stop_epoch @ gateway/run_busy.py:_route_stop_epoch */
static json_t *rb2_route_stop_epoch(rb2_env *e, const json_t *in) {
    const char *key = json_get_str(in, "session_key", "");
    json_t *ar = rb2_call(e, "active_authority", json_object());
    if (rb2_is_raise(ar)) { e->exc = ar; return json_null(); }
    if (gflow_is_null(ar)) { json_free(ar); return json_number(0); }
    json_t *a = json_object();
    json_set(a, "session_key", json_string(key));
    json_free(ar);
    json_t *rr = rb2_call(e, "route_epoch", a);
    if (rb2_is_raise(rr)) { e->exc = rr; return json_null(); }
    return rr ? rr : json_number(0);
}

/* PoP: rb2_send_busy_ack_reply @ gateway/run_busy.py:_send_busy_ack_reply */
static json_t *rb2_send_busy_ack_reply(rb2_env *e, const json_t *in) {
    json_t *a = json_object();
    const char *eid = json_get_str(in, "event_id", "");
    const char *aid = json_get_str(in, "adapter_id", NULL);
    json_set(a, "event_id", json_string(eid));
    if (aid) json_set(a, "adapter_id", json_string(aid));
    const char *msg = json_get_str(in, "message", "");
    json_set(a, "message", json_string(msg));
    json_t *r = rb2_call(e, "send_busy_reply", a);
    if (rb2_is_raise(r)) {
        if (rb2_exc_is_exception(rb2_kind(r))) {
            rb2_log(e, 10, "Failed to send busy-ack: %s", rb2_logargs1(rb2_msg(r)));
            json_free(r);
            return json_null();
        }
        e->exc = r; return NULL;   /* BaseException-only propagates */
    }
    json_free(r);
    return json_null();
}
