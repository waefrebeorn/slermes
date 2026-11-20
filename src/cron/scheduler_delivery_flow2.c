/* scheduler_delivery_flow2.c — FLOW port of the 31 UNCLAIMED cron/scheduler_delivery.py defs.
 *
 * Complements port_scheduler_flow.c / scheduler_delivery.c / scheduler_gaps.c: this
 * module ports only the defs that carried NO PoP claim (CRLF-aware PoP grep, 31 names):
 *   _resolve_cron_surface_mode, _fresh_thread_overrides_origin_topic, _origin_thread_is,
 *   _resolve_origin, _redact_cron_payload, _cron_display_name, _cron_mirror_message,
 *   _seed_cron_session, _home_env_lookup, _relay_fronted_delivery_platforms, _home_target,
 *   _get_standalone_send_timeout, bot_chat_message, _deliver_to_bot_chat,
 *   _resolve_bot_chat_target, _result_field, _note_target_error, _warn_live_lane_failure,
 *   _resolve_target_transport, _inchannel_surface_supported, _named_dm_topic,
 *   _live_route_metadata, _live_send_text, _resolve_named_dm_topic, _live_send_media,
 *   _seed_live_delivery_sessions, _deliver_via_live_adapter, _deliver_standalone,
 *   _prepare_target_delivery, _unresolved_delivery_outcome, _should_wrap_cron_response.
 *
 * Seam model (harness-emits / oracle-diffs, NAME-ONLY trace): every module-level call,
 * claimed sibling, unclaimed sibling and inner-imported dependency goes through
 * sd2_io_t.call(io, name, args). The dispatcher appends `name` to io->trace, returns the
 * scripted result (package {"ret":..,"raise":{type,message},"state":{..}}), applies
 * "state" effects onto io->state, and models a raise as {"__raise__","message"}.
 * Seam results are caller-owned (json_free); out-params are borrowed and echoed.
 *
 * Full builtin exception hierarchy: a complete name->parent table drives subclass tests,
 * so `except Exception` catches every non-BaseException-only kind; OSError-family and
 * ValueError/KeyError/TypeError gates use the same table.
 *
 * Code-only port (winlibs gcc -fsyntax-only); no build/, registry.json or libjson edits.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct sd2_io_t {
    json_t *(*call)(struct sd2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} sd2_io_t;

/* ── trace / seam plumbing ─────────────────────────────────────────────────── */
static void sd2_trace(sd2_io_t *io, const char *tok) {
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, json_string(tok));
}
static json_t *sd2_call(sd2_io_t *io, const char *name, json_t *args) {
    return (io && io->call) ? io->call(io, name, args) : json_null();
}
/* Fire one seam. name-only trace; args freed here. Returns caller-owned result. */
static json_t *sd2_seam(sd2_io_t *io, const char *name, json_t *args) {
    sd2_trace(io, name);
    json_t *r = sd2_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}
static json_t *sd2_args(void) { return json_object(); }

/* ── exception hierarchy ───────────────────────────────────────────────────── */
static bool sd2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *sd2_exc_kind(const json_t *e) {
    return json_get_str(e ? e : NULL, "__raise__", "Exception");
}
static const char *sd2_exc_msg(const json_t *e) {
    return json_get_str(e ? e : NULL, "message", "");
}
/* Construct a raise descriptor. */
static json_t *sd2_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type ? type : "Exception"));
    json_set(e, "message", json_string(msg ? msg : ""));
    return e;
}
/* Full builtin exception parent table (name -> parent). */
static const char *sd2_parent(const char *k) {
    if (!k) return NULL;
    static const char *const ose[] = {
        "OSError","EnvironmentError","IOError","WindowsError","FileNotFoundError",
        "PermissionError","FileExistsError","InterruptedError","IsADirectoryError",
        "NotADirectoryError","BlockingIOError","ChildProcessError","ConnectionError",
        "BrokenPipeError","ConnectionAbortedError","ConnectionRefusedError",
        "ConnectionResetError","TimeoutError","ProcessLookupError"
    };
    for (size_t i = 0; i < sizeof ose / sizeof ose[0]; i++)
        if (strcmp(k, ose[i]) == 0) return (i == 0) ? "Exception" : "OSError";
    struct P { const char *k, *p; };
    static const struct P tab[] = {
        {"BaseException",NULL},{"Exception","BaseException"},
        {"GeneratorExit","BaseException"},{"KeyboardInterrupt","BaseException"},{"SystemExit","BaseException"},
        {"ArithmeticError","Exception"},{"AssertionError","Exception"},{"AttributeError","Exception"},
        {"BufferError","Exception"},{"BytesWarning","Warning"},{"DeprecationWarning","Warning"},
        {"EOFError","Exception"},{"EncodingWarning","Warning"},{"FloatingPointError","ArithmeticError"},
        {"FutureWarning","Warning"},{"ImportError","Exception"},{"ImportWarning","Warning"},
        {"IndentationError","SyntaxError"},{"IndexError","LookupError"},{"LookupError","Exception"},
        {"KeyError","LookupError"},{"MemoryError","Exception"},{"ModuleNotFoundError","ImportError"},
        {"NameError","Exception"},{"NotImplementedError","RuntimeError"},{"OverflowError","ArithmeticError"},
        {"PendingDeprecationWarning","Warning"},{"RecursionError","RuntimeError"},{"ReferenceError","Exception"},
        {"ResourceWarning","Warning"},{"RuntimeError","Exception"},{"RuntimeWarning","Warning"},
        {"StopAsyncIteration","Exception"},{"StopIteration","Exception"},{"SyntaxError","Exception"},
        {"SyntaxWarning","Warning"},{"SystemError","Exception"},{"TabError","SyntaxError"},
        {"TypeError","Exception"},{"UnboundLocalError","NameError"},{"UnicodeError","ValueError"},
        {"UnicodeDecodeError","UnicodeError"},{"UnicodeEncodeError","UnicodeError"},
        {"UnicodeTranslateError","UnicodeError"},{"UnicodeWarning","Warning"},{"UserWarning","Warning"},
        {"ValueError","Exception"},{"Warning","Exception"},{"ZeroDivisionError","ArithmeticError"},
        {"ExceptionGroup","Exception"},{"BaseExceptionGroup","BaseException"}
    };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strcmp(k, tab[i].k) == 0) return tab[i].p;
    /* Unknown/custom kinds: treat as direct Exception subclass (fail-closed to caught). */
    return "Exception";
}
static bool sd2_is_subclass(const char *kind, const char *base) {
    const char *k = kind;
    for (int i = 0; i < 12 && k; i++) {
        if (strcmp(k, base) == 0) return true;
        k = sd2_parent(k);
    }
    return false;
}
/* `except Exception` catches every non-BaseException-only kind. */
static bool sd2_caught(const char *k) { return sd2_is_subclass(k, "Exception"); }
/* `except OSError` / suppress(OSError,...). */
static bool sd2_oserror(const char *k) { return sd2_is_subclass(k, "OSError"); }
/* specific tuple gate: sd2_exc_is(k,"ValueError","KeyError",NULL) */
static bool sd2_exc_is(const char *k, ...) {
    va_list ap; va_start(ap, k);
    const char *x;
    bool found = false;
    while ((x = va_arg(ap, const char *)) != NULL) {
        if (strcmp(k, x) == 0) { found = true; break; }
    }
    va_end(ap);
    return found;
}

/* ── generic value helpers ─────────────────────────────────────────────────── */
static char *sd2_strdup(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = (char *)malloc(n + 1);
    if (o) memcpy(o, p, n + 1);
    return o;
}
static bool sd2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static const char *sd2_get_str(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
/* VALUE-node string accessor (json_get_str is an OBJECT accessor only). */
static const char *sd2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static const char *sd2_sval_def(const json_t *v, const char *d) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : d;
}
static json_t *sd2_get_obj(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
/* copy a maybe-absent node as explicit JSON null (json_set(obj,k,NULL) is a no-op). */
static json_t *sd2_copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}
static json_t *sd2_fmt(const char *fmt, ...) {
    char buf[8192];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return json_string(buf);
}
/* python str() for the scalar fixtures we use: bool->True/False, null->None,
 * string->as-is, number->%g-ish (integers print without .0). */
static json_t *sd2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return json_string("None");
    switch (v->type) {
        case JSON_BOOL: return json_string(v->bool_val ? "True" : "False");
        case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            if (v->num_val == (double)(long long)v->num_val)
                return sd2_fmt("%lld", (long long)v->num_val);
            return sd2_fmt("%g", v->num_val);
        }
        default: return json_copy(v);
    }
}
/* an "object" double is {"__attr__": {...}}; plain JSON object is a dict. */
static json_t *sd2_attr_inner(const json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        json_t *w = json_obj_get(v, "__attr__");
        if (w && w->type == JSON_OBJECT) return w;
    }
    return NULL;
}
static bool sd2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool sd2_is_number(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sd2_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT && !sd2_attr_inner(v); }


/* python str.strip() (White_Space) */
static const char *sd2_strip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) n--;
    return s; /* NOTE: trailing trim applied by caller via bounded copy */
}
static char *sd2_stripped_copy(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) n--;
    char *o = (char *)malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
/* ── helpers: python str().strip().lower() == target ────────────────────────── */
static bool sd2_strip_lower_eq(const char *s, const char *target) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) n--;
    if (n != strlen(target)) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)target[i])) return false;
    return true;
}
static const char *sd2_lower_copy(const char *s, char *buf, size_t cap) {
    size_t n = strlen(s ? s : "");
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; i++) buf[i] = tolower((unsigned char)s[i]);
    buf[n] = '\0';
    return buf;
}
/* python `a or b` for json nodes (returns first truthy operand). */
static json_t *sd2_or(json_t *a, json_t *b) {
    return sd2_truthy(a) ? a : b;
}

/* 1. _resolve_cron_surface_mode */
/* PoP: sd2_resolve_cron_surface_mode @ cron/scheduler_delivery.py:_resolve_cron_surface_mode */
static json_t *sd2_resolve_cron_surface_mode(sd2_io_t *io, json_t *pconfig, json_t *logical_name) {
    (void)io;
    json_t *extra = json_obj_get(pconfig, "extra");
    if (!sd2_truthy(extra)) extra = NULL;
    if (extra && extra->type != JSON_OBJECT) return json_string("thread"); /* suppress(Exception) */
    const char *ln = (logical_name && logical_name->type == JSON_STRING && logical_name->str_val)
                     ? logical_name->str_val : "";
    char key[512]; sd2_lower_copy(ln, key, sizeof key);
    json_t *raw = NULL;
    if (extra) {
        json_t *sub = json_obj_get(extra, key);
        if (sub && sub->type == JSON_OBJECT) raw = json_obj_get(sub, "cron_continuable_surface");
        if (!raw || raw->type == JSON_NULL) raw = json_obj_get(extra, "cron_continuable_surface");
    }
    if (raw && raw->type != JSON_NULL) {
        json_t *pv = sd2_pystr(raw);
        bool eq = sd2_strip_lower_eq(sd2_sval(pv), "in_channel");
        json_free(pv);
        if (eq) return json_string("in_channel");
    }
    return json_string("thread");
}

/* 2. _fresh_thread_overrides_origin_topic */
/* PoP: sd2_fresh_thread_overrides_origin_topic @ cron/scheduler_delivery.py:_fresh_thread_overrides_origin_topic */
static json_t *sd2_fresh_thread_overrides_origin_topic(sd2_io_t *io, json_t *pconfig) {
    json_t *extra = json_obj_get(pconfig, "extra");
    if (!sd2_truthy(extra)) extra = NULL;
    json_t *raw = NULL;
    if (extra && extra->type == JSON_OBJECT) {
        raw = json_obj_get(extra, "cron_fresh_thread_per_run");
        if (!raw) raw = json_bool(false);
    } else {
        raw = json_bool(false);
    }
    if (raw && raw->type == JSON_BOOL) return json_bool(raw->bool_val);
    json_t *a = sd2_args();
    json_set(a, "raw", sd2_copy_or_null(raw));
    json_free(sd2_seam(io, "logger.warning", a));
    return json_bool(false);
}

/* 3. _origin_thread_is */
/* PoP: sd2_origin_thread_is @ cron/scheduler_delivery.py:_origin_thread_is */
static json_t *sd2_origin_thread_is(sd2_io_t *io, json_t *job, json_t *thread_id) {
    json_t *r = sd2_seam(io, "resolve_origin", sd2_args());
    if (sd2_is_raise(r)) return r;
    json_t *origin = (r && r->type == JSON_OBJECT) ? r : NULL;
    json_t *ot = origin ? json_obj_get(origin, "thread_id") : NULL;
    bool ok = false;
    if (ot && ot->type != JSON_NULL) {
        json_t *a = sd2_pystr(ot), *b = sd2_pystr(thread_id);
        ok = strcmp(sd2_sval(a), sd2_sval(b)) == 0;
        json_free(a); json_free(b);
    }
    json_free(r);
    return json_bool(ok);
}

/* 4. _resolve_origin */
/* PoP: sd2_resolve_origin @ cron/scheduler_delivery.py:_resolve_origin */
static json_t *sd2_resolve_origin(sd2_io_t *io, json_t *job) {
    (void)io;
    json_t *origin = json_obj_get(job, "origin");
    if (!origin || origin->type != JSON_OBJECT || sd2_attr_inner(origin)) return json_null();
    json_t *plat = json_obj_get(origin, "platform");
    json_t *chat = json_obj_get(origin, "chat_id");
    if (!sd2_truthy(plat) || !sd2_truthy(chat)) return json_null();
    char low[256];
    json_t *pv = sd2_pystr(plat);
    sd2_lower_copy(sd2_sval(pv), low, sizeof low);
    json_free(pv);
    if (strcmp(low, "api_server") == 0) return json_null();
    return json_copy(origin);
}

/* 5. _redact_cron_payload */
/* PoP: sd2_redact_cron_payload @ cron/scheduler_delivery.py:_redact_cron_payload */
static json_t *sd2_redact_cron_payload(sd2_io_t *io, json_t *text, json_t *what) {
    if (!sd2_truthy(text)) return sd2_copy_or_null(text);
    json_t *a = sd2_args();
    json_set(a, "text", sd2_copy_or_null(text));
    json_set(a, "force", json_bool(true));
    json_t *r = sd2_seam(io, "redact_sensitive_text", a);
    if (sd2_is_raise(r)) {
        if (sd2_caught(sd2_exc_kind(r))) {
            json_t *w = sd2_args();
            json_set(w, "what", sd2_copy_or_null(what));
            json_set(w, "e", json_string(sd2_exc_msg(r)));
            json_free(sd2_seam(io, "logger.warning", w));
            json_free(r);
            return json_string("[REDACTED - redaction failed]");
        }
        return r; /* propagate BaseException-only */
    }
    return r;
}

/* 6. _cron_display_name */
/* PoP: sd2_cron_display_name @ cron/scheduler_delivery.py:_cron_display_name */
static json_t *sd2_cron_display_name(sd2_io_t *io, json_t *job) {
    json_t *name = json_obj_get(job, "name");
    if (!sd2_truthy(name)) name = sd2_or(json_obj_get(job, "id"), json_string("cron"));
    json_t *a = sd2_args();
    json_set(a, "text", sd2_copy_or_null(name));
    json_set(a, "what", json_string("job name"));
    return sd2_seam(io, "redact_cron_payload", a);
}

/* 7. _cron_mirror_message */
/* PoP: sd2_cron_mirror_message @ cron/scheduler_delivery.py:_cron_mirror_message */
static json_t *sd2_cron_mirror_message(sd2_io_t *io, json_t *job, json_t *text) {
    json_t *r = sd2_seam(io, "cron_display_name", sd2_args());
    if (sd2_is_raise(r)) return r;
    json_t *ps = sd2_pystr(r);
    json_t *out = sd2_fmt("[Cron delivery: %s]\n%s", sd2_sval(ps),
                          text && text->type == JSON_STRING ? sd2_sval(text) : "");
    json_free(ps); json_free(r);
    return out;
}

/* 8a. _seed_cron_session -> bool */
/* PoP: sd2_seed_cron_session @ cron/scheduler_delivery.py:_seed_cron_session */
static json_t *sd2_seed_cron_session(sd2_io_t *io, json_t *job, json_t *adapter,
        json_t *platform_name, json_t *chat_id, json_t *text, json_t *thread_id,
        json_t *chat_type, json_t *user_id, json_t *user_name, json_t *chat_name,
        json_t *scope_id, bool discord_keys_on_thread) {
    json_t *seeded_session_id = NULL;
    json_t *store = json_obj_get(adapter, "_session_store");
    if (store && store->type != JSON_NULL) {
        json_t *a0 = sd2_args();
        json_set(a0, "platform_name", sd2_copy_or_null(platform_name));
        json_t *penum = sd2_seam(io, "Platform", a0);
        if (sd2_is_raise(penum)) {
            const char *k = sd2_exc_kind(penum);
            if (!sd2_exc_is(k, "ValueError", "KeyError", NULL)) return penum;
            json_free(penum); penum = NULL;
        }
        if (penum && penum->type != JSON_NULL) {
            const char *pl = sd2_sval(penum);
            bool use_thread = discord_keys_on_thread && strcmp(pl, "discord") == 0;
            json_t *seed_chat_id = use_thread ? sd2_pystr(thread_id) : sd2_pystr(chat_id);
            json_t *a1 = sd2_args();
            json_set(a1, "platform", sd2_copy_or_null(penum));
            json_set(a1, "chat_id", sd2_copy_or_null(seed_chat_id));
            json_set(a1, "chat_name", sd2_copy_or_null(chat_name));
            json_set(a1, "chat_type", sd2_copy_or_null(chat_type));
            json_set(a1, "user_id", sd2_copy_or_null(user_id));
            json_set(a1, "user_name", sd2_copy_or_null(user_name));
            json_set(a1, "thread_id", sd2_copy_or_null(thread_id));
            json_set(a1, "scope_id", sd2_truthy(scope_id) ? sd2_pystr(scope_id) : json_null());
            json_free(sd2_seam(io, "SessionSource", a1));
            json_free(seed_chat_id);
            json_t *a2 = sd2_args(); json_set(a2, "source", json_string("dest_source"));
            json_t *entry = sd2_seam(io, "get_or_create_session", a2);
            if (sd2_is_raise(entry)) { json_free(penum); return entry; }
            seeded_session_id = json_obj_get(entry, "session_id");
            if (seeded_session_id) seeded_session_id = json_copy(seeded_session_id);
            else seeded_session_id = json_null();
            json_free(entry);
            json_free(penum);
        }
    }
    json_t *a3 = sd2_args(); json_set(a3, "job", json_copy(job)); json_set(a3, "text", sd2_copy_or_null(text));
    json_t *msg = sd2_seam(io, "cron_mirror_message", a3);
    if (sd2_is_raise(msg)) { json_free(seeded_session_id); return msg; }
    json_t *a4 = sd2_args();
    json_set(a4, "platform", sd2_copy_or_null(platform_name));
    json_set(a4, "chat_id", sd2_pystr(chat_id));
    json_set(a4, "message", sd2_copy_or_null(msg));
    json_set(a4, "source_label", json_string("cron"));
    json_set(a4, "thread_id", sd2_copy_or_null(thread_id));
    json_set(a4, "user_id", sd2_copy_or_null(user_id));
    json_set(a4, "role", json_string("user"));
    json_set(a4, "session_id", sd2_copy_or_null(seeded_session_id));
    json_free(msg); json_free(seeded_session_id);
    return sd2_seam(io, "mirror_to_session", a4);
}

/* 8. _result_field */
/* PoP: sd2_result_field @ cron/scheduler_delivery.py:_result_field */
static json_t *sd2_result_field(sd2_io_t *io, json_t *send_result, json_t *key, json_t *def) {
    (void)io;
    const char *k = sd2_sval(key);
    json_t *inner = sd2_attr_inner(send_result);
    json_t *v;
    if (inner) v = json_obj_get(inner, k);
    else if (send_result && send_result->type == JSON_OBJECT) v = json_obj_get(send_result, k);
    else v = NULL;
    return sd2_copy_or_null(v ? v : def);
}

/* 9. _note_target_error (echoes errors) */
/* PoP: sd2_note_target_error @ cron/scheduler_delivery.py:_note_target_error */
static json_t *sd2_note_target_error(sd2_io_t *io, json_t *job, json_t *msg, json_t *errors) {
    json_t *a = sd2_args();
    json_set(a, "id", sd2_copy_or_null(json_obj_get(job, "id")));
    json_set(a, "msg", sd2_copy_or_null(msg));
    json_t *r = sd2_seam(io, "logger.warning", a);
    if (sd2_is_raise(r)) return r;
    json_free(r);
    if (errors && errors->type == JSON_ARRAY) json_append(errors, sd2_copy_or_null(msg));
    return json_null();
}

/* 10. _warn_live_lane_failure */
/* PoP: sd2_warn_live_lane_failure @ cron/scheduler_delivery.py:_warn_live_lane_failure */
static json_t *sd2_warn_live_lane_failure(sd2_io_t *io, json_t *job, json_t *msg, bool is_relay) {
    json_t *a = sd2_args();
    json_set(a, "id", sd2_copy_or_null(json_obj_get(job, "id")));
    json_set(a, "msg", sd2_copy_or_null(msg));
    json_set(a, "is_relay", json_bool(is_relay));
    json_free(sd2_seam(io, "logger.warning", a));
    return json_null();
}

/* 11. _inchannel_surface_supported */
/* PoP: sd2_inchannel_surface_supported @ cron/scheduler_delivery.py:_inchannel_surface_supported */
static json_t *sd2_inchannel_surface_supported(sd2_io_t *io, json_t *adapter, json_t *platform_name) {
    bool has = false;
    json_t *flag = json_obj_get(adapter, "per_platform");
    if (flag && flag->type == JSON_BOOL && flag->bool_val) has = true;
    if (has) {
        json_t *a = sd2_args();
        json_set(a, "platform", sd2_copy_or_null(platform_name));
        json_t *r = sd2_seam(io, "supports_inchannel_continuable_for_platform", a);
        if (sd2_is_raise(r)) {
            if (sd2_caught(sd2_exc_kind(r))) { json_free(r); return json_bool(false); }
            return r;
        }
        bool v = sd2_truthy(r);
        json_free(r);
        return json_bool(v);
    }
    return json_bool(sd2_truthy(json_obj_get(adapter, "supports")));
}

/* 12. _should_wrap_cron_response */
/* PoP: sd2_should_wrap_cron_response @ cron/scheduler_delivery.py:_should_wrap_cron_response */
static json_t *sd2_should_wrap_cron_response(sd2_io_t *io, json_t *job, json_t *user_cfg) {
    (void)io;
    json_t *w = json_obj_get(job, "wrap_response");
    if (w && w->type == JSON_BOOL) return json_bool(w->bool_val);
    json_t *cron_cfg = NULL;
    if (user_cfg && user_cfg->type == JSON_OBJECT) cron_cfg = json_obj_get(user_cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) return json_bool(true);
    json_t *v = json_obj_get(cron_cfg, "wrap_response");
    if (!v) v = json_bool(true);
    return json_bool(sd2_truthy(v));
}


/* python int(x) for scalar fixtures; ok=false on ValueError/TypeError. */
static long long sd2_pyint(const json_t *v, bool *ok) {
    *ok = true;
    if (!v) { *ok = false; return 0; }
    switch (v->type) {
        case JSON_NUMBER: return (long long)v->num_val;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            char *end = NULL;
            long long n = strtoll(s, &end, 10);
            if (end == s) { *ok = false; return 0; }
            while (*end && isspace((unsigned char)*end)) end++;
            if (*end) { *ok = false; return 0; }
            return n;
        }
        default: *ok = false; return 0;
    }
}

/* 13. _get_standalone_send_timeout */
/* PoP: sd2_get_standalone_send_timeout @ cron/scheduler_delivery.py:_get_standalone_send_timeout */
static json_t *sd2_get_standalone_send_timeout(sd2_io_t *io) {
    json_t *r = sd2_seam(io, "load_config", sd2_args());
    if (sd2_is_raise(r)) { json_free(r); return json_number(60); }
    json_t *cron = (r && r->type == JSON_OBJECT) ? json_obj_get(r, "cron") : NULL;
    json_t *val = NULL;
    if (cron && cron->type == JSON_OBJECT)
        val = json_obj_get(cron, "standalone_send_timeout_seconds");
    if (!val) val = json_number(60);
    bool ok = false;
    long long n = sd2_pyint(val, &ok);
    json_free(r);
    if (!ok) return json_number(60);
    return json_number(n > 0 ? (double)n : 60.0);
}

/* 14. _home_env_lookup */
static const char *sd2_legacy_home(const char *env_var) {
    if (strcmp(env_var, "QQBOT_HOME_CHANNEL") == 0) return "QQ_HOME_CHANNEL";
    return NULL;
}
/* PoP: sd2_home_env_lookup @ cron/scheduler_delivery.py:_home_env_lookup */
static json_t *sd2_home_env_lookup(sd2_io_t *io, json_t *env_var, json_t *suffix, bool strip) {
    const char *ev = sd2_sval(env_var);
    const char *sf = sd2_sval(suffix);
    char name[512];
    snprintf(name, sizeof name, "%s%s", ev, sf);
    /* read(env_var+suffix) */
    json_t *a = sd2_args(); json_set(a, "name", json_string(name));
    json_t *r = sd2_seam(io, "get_secret", a);
    if (sd2_is_raise(r)) return r;
    const char *sv = sd2_sval(r);
    json_t *value;
    if (strip) { char *sc = sd2_stripped_copy(sv); value = json_string(sc ? sc : ""); free(sc); }
    else value = json_string(sv ? sv : "");
    json_free(r);
    if (!sd2_truthy(value)) {
        const char *legacy = sd2_legacy_home(ev);
        if (legacy) {
            char n2[512]; snprintf(n2, sizeof n2, "%s%s", legacy, sf);
            json_t *a2 = sd2_args(); json_set(a2, "name", json_string(n2));
            json_t *r2 = sd2_seam(io, "get_secret", a2);
            if (sd2_is_raise(r2)) { json_free(value); return r2; }
            const char *s2 = sd2_sval(r2);
            json_free(value);
            if (strip) { char *sc = sd2_stripped_copy(s2); value = json_string(sc ? sc : ""); free(sc); }
            else value = json_string(s2 ? s2 : "");
            json_free(r2);
        }
    }
    return value;
}

/* 15. _relay_fronted_delivery_platforms */
/* PoP: sd2_relay_fronted_delivery_platforms @ cron/scheduler_delivery.py:_relay_fronted_delivery_platforms */
static json_t *sd2_relay_fronted_delivery_platforms(sd2_io_t *io, json_t *connected) {
    bool in_relay = false;
    if (connected && connected->type == JSON_ARRAY) {
        for (size_t i = 0; i < connected->c.count; i++) {
            json_t *e = json_get(connected, i);
            if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "relay") == 0) {
                in_relay = true; break;
            }
        }
    }
    if (!in_relay) return json_array();
    json_t *r = sd2_seam(io, "relay_fronted_platforms", sd2_args());
    if (sd2_is_raise(r)) {
        if (sd2_caught(sd2_exc_kind(r))) {
            json_free(sd2_seam(io, "logger.debug", sd2_args()));
            json_free(r);
            return json_array();
        }
        return r;
    }
    return r;
}

/* 16. _home_target */
/* PoP: sd2_home_target @ cron/scheduler_delivery.py:_home_target */
static json_t *sd2_home_target(sd2_io_t *io, json_t *platform_name, json_t *chat_id, json_t *resolved_from) {
    json_t *r = sd2_seam(io, "get_home_target_thread_id", sd2_args());
    if (sd2_is_raise(r)) return r;
    json_t *t = json_object();
    json_set(t, "platform", sd2_copy_or_null(platform_name));
    json_set(t, "chat_id", sd2_copy_or_null(chat_id));
    json_set(t, "thread_id", sd2_copy_or_null(r));
    json_free(r);
    if (sd2_truthy(resolved_from))
        json_set(t, "_resolved_from", sd2_copy_or_null(resolved_from));
    return t;
}

/* helper: job.get(key, fallback) where present-null wins (python .get) */
static json_t *sd2_dict_get_default(json_t *obj, const char *k, json_t *dflt) {
    json_t *v = json_obj_get(obj, k);
    return v ? v : dflt;
}

/* 17. bot_chat_message */
/* PoP: sd2_bot_chat_message @ cron/scheduler_delivery.py:bot_chat_message */
static json_t *sd2_bot_chat_message(sd2_io_t *io, json_t *job, json_t *content) {
    json_t *a = sd2_args();
    json_set(a, "text", sd2_copy_or_null(content));
    json_set(a, "what", json_string("bot-chat payload"));
    json_t *c = sd2_seam(io, "redact_cron_payload", a);
    if (sd2_is_raise(c)) return c;
    json_t *nm = sd2_dict_get_default(job, "name", sd2_dict_get_default(job, "id", json_string("?")));
    json_t *b = sd2_args();
    json_set(b, "text", sd2_copy_or_null(nm));
    json_set(b, "what", json_string("job name"));
    json_t *jn = sd2_seam(io, "redact_cron_payload", b);
    if (sd2_is_raise(jn)) { json_free(c); return jn; }
    const char *cs = sd2_sval(c);
    const char *ns = sd2_sval(jn);
    json_t *out = sd2_fmt("[Cronjob \"%s\" output \xe2\x80\x94 scheduled job, not the user. "
                          "Review it, act on anything that needs action, and summarize for the chat.]\n\n%s",
                          ns ? ns : "", cs ? cs : "");
    json_free(c); json_free(jn);
    return out;
}

/* 18. _deliver_to_bot_chat (echoes job) */
/* PoP: sd2_deliver_to_bot_chat @ cron/scheduler_delivery.py:_deliver_to_bot_chat */
static json_t *sd2_deliver_to_bot_chat(sd2_io_t *io, json_t *job, json_t *content,
                                        json_t *profile, bool for_failure) {
    const char *jid = sd2_sval_def(json_obj_get(job, "id"), "?");
    const char *plabel = sd2_truthy(profile) ? sd2_sval(profile) : "(own)";
    json_t *pending = NULL;
    /* message = bot_chat_message(...) -- OUTSIDE try */
    json_t *a0 = sd2_args();
    json_set(a0, "job", json_copy(job));
    json_set(a0, "content", sd2_copy_or_null(content));
    json_t *message = sd2_seam(io, "bot_chat_message", a0);
    if (sd2_is_raise(message)) return message;

    /* --- try --- */
    json_t *home = sd2_seam(io, "get_hermes_home", sd2_args());
    if (sd2_is_raise(home)) { pending = home; goto dtc_except; }
    json_t *a1 = sd2_args(); json_set(a1, "profile", sd2_copy_or_null(profile));
    json_t *h = sd2_truthy(profile) ? sd2_seam(io, "get_profile_dir", a1) : NULL;
    if (h && sd2_is_raise(h)) { pending = h; goto dtc_except; }
    const char *home_s = h ? sd2_sval(h) : sd2_sval(home);

    bool suppress_notification = false;
    if (for_failure) {
        json_t *a2 = sd2_args(); json_set(a2, "home", json_string(home_s ? home_s : ""));
        json_free(sd2_seam(io, "load_user_config_effective", a2));
        json_t *a3 = sd2_args(); json_set(a3, "platform", json_string("bot-chat"));
        json_t *wn = sd2_seam(io, "warning_notifications_enabled", a3);
        if (sd2_is_raise(wn)) { pending = wn; goto dtc_except; }
        suppress_notification = !sd2_truthy(wn);
        json_free(wn);
    }

    json_t *run_id = json_obj_get(job, "execution_id");
    if (!sd2_truthy(run_id)) {
        json_t *a4 = sd2_args();
        json_t *u = sd2_seam(io, "uuid4", a4);
        if (sd2_is_raise(u)) { pending = u; goto dtc_except; }
        run_id = u; /* owned */
        json_t *rcv = sd2_get_obj(job, "_bot_chat_run_id"); (void)rcv;
        json_set(job, "_bot_chat_run_id", sd2_copy_or_null(u));
    }
    const char *run_s = sd2_sval(run_id);
    json_t *a5 = sd2_args();
    json_set(a5, "source_home", json_string(home_s ? home_s : ""));
    json_set(a5, "job_id", json_string(jid ? jid : "?"));
    json_set(a5, "run_id", json_string(run_s ? run_s : ""));
    json_set(a5, "home", json_string(home_s ? home_s : ""));
    json_t *keyr = sd2_seam(io, "sha256", a5);
    if (sd2_is_raise(keyr)) { pending = keyr; goto dtc_except; }
    const char *key = sd2_sval(keyr);
    char keybuf[512]; snprintf(keybuf, sizeof keybuf, "%s", key ? key : "");

    json_t *a6 = sd2_args();
    json_set(a6, "home", json_string(home_s ? home_s : ""));
    json_set(a6, "key", json_string(keybuf));
    json_t *receipt = sd2_seam(io, "read_delivery_result", a6);
    if (sd2_is_raise(receipt)) { pending = receipt; goto dtc_except; }
    bool receipt_none = (receipt && receipt->type == JSON_NULL) || !receipt;
    json_t *target = sd2_fmt("bot-chat:%s", plabel);
    const char *target_s = sd2_sval(target);

    if (receipt_none && suppress_notification) {
        json_t *rc = sd2_get_obj(job, "_bot_chat_delivery_receipts");
        if (!rc) { rc = json_object(); json_set(job, "_bot_chat_delivery_receipts", rc); }
        json_t *ent = json_object();
        json_set(ent, "status", json_string("suppressed"));
        json_set(ent, "delivery_id", json_string(keybuf));
        json_set(rc, target_s, ent);
        json_set(job, "_notification_all_targets_suppressed", json_bool(true));
        json_free(receipt); json_free(target); json_free(keyr);
        if (!sd2_truthy(run_id) == false) { /* no-op */ }
        if (run_id == json_obj_get(job, "execution_id")) { /* borrowed */ }
        else json_free(run_id);
        json_free(home); if (h) json_free(h); json_free(message);
        return json_null();
    }
    if (receipt_none) {
        json_t *a7 = sd2_args(); json_set(a7, "home", json_string(home_s ? home_s : ""));
        json_t *owner = sd2_seam(io, "find_canonical_live_owner", a7);
        if (sd2_is_raise(owner)) {
            if (strcmp(sd2_exc_kind(owner), "ValueError") == 0) {
                json_t *a8 = sd2_args();
                json_set(a8, "key", json_string(keybuf));
                json_set(a8, "job", json_copy(job));
                json_set(a8, "content", sd2_copy_or_null(content));
                json_set(a8, "home", json_string(home_s ? home_s : ""));
                json_set(a8, "for_failure", json_bool(for_failure));
                json_t *dr = sd2_seam(io, "defer", a8);
                if (sd2_is_raise(dr)) { pending = dr; goto dtc_except; }
                json_free(dr);
                json_t *rc = sd2_get_obj(job, "_bot_chat_delivery_receipts");
                if (!rc) { rc = json_object(); json_set(job, "_bot_chat_delivery_receipts", rc); }
                json_t *ent = json_object();
                json_set(ent, "status", json_string("queued"));
                json_set(ent, "delivery_id", json_string(keybuf));
                json_set(rc, target_s, ent);
                json_t *out = sd2_fmt("%s queued (receipt %s): target profile not running; admitted once it is",
                                      target_s, keybuf);
                json_free(owner); json_free(receipt); json_free(target); json_free(keyr);
                json_free(home); if (h) json_free(h); json_free(message);
                return out;
            }
            pending = owner; goto dtc_except;
        }
        if (!sd2_truthy(owner)) {
            json_t *out = sd2_fmt("bot-chat delivery to profile '%s' unverified: no canonical Bot Chat", plabel);
            json_free(owner); json_free(receipt); json_free(target); json_free(keyr);
            json_free(home); if (h) json_free(h); json_free(message);
            return out;
        }
        json_t *a9 = sd2_args();
        json_set(a9, "home", json_string(home_s ? home_s : ""));
        json_set(a9, "owner", sd2_copy_or_null(owner));
        json_set(a9, "message", sd2_copy_or_null(message));
        json_set(a9, "delivery_id", json_string(keybuf));
        json_set(a9, "notification_category", json_string(for_failure ? "diagnostic" : "result"));
        json_free(receipt);
        receipt = sd2_seam(io, "deliver_to_live_owner", a9);
        if (sd2_is_raise(receipt)) { pending = receipt; goto dtc_except; }
        json_free(owner);
    }
    /* verify message/category */
    json_t *rmsg = json_obj_get(receipt, "message");
    const char *rs = sd2_sval(rmsg);
    const char *ms = sd2_sval(message);
    json_t *rcat = json_obj_get(receipt, "notification_category");
    const char *cat = rcat ? sd2_sval_def(rcat, "result") : "result";
    const char *wantcat = for_failure ? "diagnostic" : "result";
    bool bad = (strcmp(rs ? rs : "", ms ? ms : "") != 0) || (strcmp(cat, wantcat) != 0);
    if (bad) {
        pending = sd2_raise("ValueError", "delivery id already belongs to a different payload");
        goto dtc_except;
    }
    json_t *st = json_obj_get(receipt, "status");
    const char *status = sd2_sval(st);
    json_t *rc = sd2_get_obj(job, "_bot_chat_delivery_receipts");
    if (!rc) { rc = json_object(); json_set(job, "_bot_chat_delivery_receipts", rc); }
    json_t *ent = json_object();
    json_set(ent, "status", sd2_copy_or_null(st));
    json_set(ent, "delivery_id", json_string(keybuf));
    json_set(rc, target_s, ent);
    json_t *ali = sd2_args();
    json_set(ali, "job_id", json_string(jid ? jid : "?"));
    json_set(ali, "profile_label", json_string(plabel));
    json_set(ali, "key", json_string(keybuf));
    json_set(ali, "status", sd2_copy_or_null(st));
    json_free(sd2_seam(io, "logger.info", ali));
    if (status && strcmp(status, "settled") == 0) {
        json_free(receipt); json_free(target); json_free(keyr);
        json_free(home); if (h) json_free(h); json_free(message);
        if (run_id != json_obj_get(job, "execution_id")) json_free(run_id);
        return json_null();
    }
    const char *detail;
    const char *derr = NULL;
    json_t *e = json_obj_get(receipt, "error"), *rsn = json_obj_get(receipt, "reason");
    if (status && (strcmp(status, "queued") == 0 || strcmp(status, "claimed") == 0))
        detail = "completion unverified; do not resend";
    else if (derr) detail = derr;
    else if (rsn) detail = sd2_sval(rsn);
    else detail = "not completed";
    (void)e;
    json_t *out = sd2_fmt("%s %s (receipt %s): %s", target_s, status ? status : "", keybuf, detail);
    json_free(receipt); json_free(target); json_free(keyr);
    json_free(home); if (h) json_free(h); json_free(message);
    if (run_id != json_obj_get(job, "execution_id")) json_free(run_id);
    return out;

dtc_except:
    json_free(message); if (home != pending) json_free(home); if (h && h != pending) json_free(h);
    if (pending && !sd2_caught(sd2_exc_kind(pending))) return pending;
    {
        json_t *out = sd2_fmt("bot-chat delivery to profile '%s' unverified: %s",
                              plabel, pending ? sd2_exc_msg(pending) : "");
        if (pending) json_free(pending);
        return out;
    }
}

/* 19. _resolve_bot_chat_target */
/* PoP: sd2_resolve_bot_chat_target @ cron/scheduler_delivery.py:_resolve_bot_chat_target */
static json_t *sd2_resolve_bot_chat_target(sd2_io_t *io, json_t *job, json_t *profile_arg) {
    if (!sd2_truthy(profile_arg)) {
        json_t *t = json_object();
        json_set(t, "platform", json_string("bot-chat"));
        json_set(t, "chat_id", json_string(""));
        json_set(t, "thread_id", json_null());
        return t;
    }
    json_t *a1 = sd2_args(); json_set(a1, "name", sd2_copy_or_null(profile_arg));
    json_t *canon = sd2_seam(io, "normalize_profile_name", a1);
    json_t *exists = NULL;
    if (sd2_is_raise(canon)) goto rbt_except;
    json_t *a2 = sd2_args(); json_set(a2, "name", sd2_copy_or_null(canon));
    exists = sd2_seam(io, "profile_exists", a2);
    if (sd2_is_raise(exists)) { goto rbt_except; }
    if (!sd2_truthy(exists)) {
        json_t *w = sd2_args();
        json_set(w, "id", sd2_copy_or_null(json_obj_get(job, "id")));
        json_set(w, "profile", sd2_copy_or_null(profile_arg));
        json_free(sd2_seam(io, "logger.warning", w));
        json_free(exists); json_free(canon);
        return json_null();
    }
    json_free(exists);
    json_t *t = json_object();
    json_set(t, "platform", json_string("bot-chat"));
    json_set(t, "chat_id", sd2_copy_or_null(canon));
    json_set(t, "thread_id", json_null());
    json_free(canon);
    return t;
rbt_except:
    {
        json_t *e = sd2_is_raise(canon) ? canon : (sd2_is_raise(exists) ? exists : NULL);
        if (e && !sd2_caught(sd2_exc_kind(e))) return e;
        json_t *w = sd2_args();
        json_set(w, "id", sd2_copy_or_null(json_obj_get(job, "id")));
        json_set(w, "profile", sd2_copy_or_null(profile_arg));
        json_free(sd2_seam(io, "logger.warning", w));
        if (e) json_free(e);
        return json_null();
    }
}


/* 20. _resolve_target_transport */
/* ret: {"ok":bool,"error":str|null,"transport":..,"pconfig":..,"runtime_adapter":..,"target_adapters":..} */
/* PoP: sd2_resolve_target_transport @ cron/scheduler_delivery.py:_resolve_target_transport */
static json_t *sd2_resolve_target_transport(sd2_io_t *io, json_t *job, json_t *platform,
        json_t *platform_name, json_t *target, json_t *adapters, json_t *config) {
    (void)job;
    bool shared_adapters = false;
    json_t *shflag = json_obj_get(adapters, "__shared__");
    if (shflag && shflag->type == JSON_BOOL && shflag->bool_val) shared_adapters = true;
    json_t *transport = NULL;
    json_t *target_adapters = json_copy(adapters);
    if (shared_adapters) {
        json_t *a = sd2_args();
        json_set(a, "platform", sd2_copy_or_null(platform));
        json_set(a, "target", sd2_copy_or_null(target));
        json_t *sh = sd2_seam(io, "shared_get", a);
        if (sd2_is_raise(sh)) return sh;
        json_t *ta = json_object();
        if (sh && sh->type != JSON_NULL) json_set(ta, sd2_sval(platform), sd2_copy_or_null(sh));
        json_free(target_adapters); target_adapters = ta;
        if (sh && sh->type != JSON_NULL) {
            json_t *a2 = sd2_args(); json_set(a2, "platform", sd2_copy_or_null(platform));
            json_t *own = sd2_seam(io, "platforms_get", a2);
            if (sd2_is_raise(own)) { json_free(sh); json_free(own); return own; }
            json_t *a3 = sd2_args(); json_set(a3, "own", sd2_copy_or_null(own));
            json_t *pc = sd2_seam(io, (own && own->type != JSON_NULL) ? "replace" : "PlatformConfig", a3);
            if (sd2_is_raise(pc)) { json_free(sh); json_free(own); json_free(pc); return pc; }
            json_t *a4 = sd2_args();
            json_set(a4, "adapter", sd2_copy_or_null(sh));
            json_set(a4, "pconfig", sd2_copy_or_null(pc));
            json_set(a4, "platform", sd2_copy_or_null(platform));
            transport = sd2_seam(io, "DeliveryTransport", a4);
            if (sd2_is_raise(transport)) { json_free(sh); json_free(own); json_free(pc); return transport; }
            json_free(sh); json_free(own); json_free(pc);
        } else {
            json_free(sh);
        }
    }
    if (!transport) {
        json_t *a = sd2_args();
        json_set(a, "platform", sd2_copy_or_null(platform));
        json_set(a, "target_adapters", json_copy(target_adapters));
        transport = sd2_seam(io, "resolve_delivery_transport", a);
        if (sd2_is_raise(transport)) return transport;
    }
    json_t *pconfig = NULL, *runtime = NULL;
    bool tnonnull = transport && transport->type == JSON_OBJECT && transport->type != JSON_NULL
                    && !sd2_is_raise(transport);
    if (tnonnull) {
        pconfig = json_obj_get(transport, "config");
        runtime = json_obj_get(transport, "adapter");
    } else {
        json_t *a = sd2_args(); json_set(a, "platforms", json_string(
            sd2_sval(platform_name)));
        json_t *r = sd2_seam(io, "relay_fronted_platforms", a);
        if (sd2_is_raise(r)) { json_free(r); json_free(transport); json_free(target_adapters); return json_null(); }
        bool in_relay = false;
        if (r && r->type == JSON_ARRAY) {
            for (size_t i = 0; i < r->c.count; i++) {
                json_t *e = json_get(r, i);
                if (e && e->type == JSON_STRING && e->str_val &&
                    strcmp(e->str_val, sd2_sval(platform_name)) == 0) { in_relay = true; break; }
            }
        }
        json_free(r);
        if (in_relay) {
            json_t *out = json_object();
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", sd2_fmt("platform '%s' is relay-fronted and has no live gateway transport; start the gateway (its ticker owns relay-fronted delivery and will fire the job on schedule)",
                                            sd2_sval(platform_name)));
            json_free(transport); json_free(target_adapters);
            return out;
        }
        json_t *a2 = sd2_args(); json_set(a2, "platform", sd2_copy_or_null(platform));
        json_t *pc = sd2_seam(io, "platforms_get", a2);
        if (sd2_is_raise(pc)) { json_free(pc); json_free(transport); json_free(target_adapters); return json_null(); }
        pconfig = pc;
        runtime = NULL;
    }
    /* relay transport or pconfig None -> PlatformConfig(enabled=True) */
    if (tnonnull) {
        bool is_relay = false;
        json_t *ir = json_obj_get(transport, "is_relay");
        if (ir && ir->type == JSON_BOOL && ir->bool_val) is_relay = true;
        if (is_relay || !pconfig || pconfig->type == JSON_NULL) {
            if (!pconfig || pconfig->type == JSON_NULL) {
                json_t *pc = sd2_seam(io, "PlatformConfig", sd2_args());
                if (sd2_is_raise(pc)) { json_free(pc); json_free(transport); json_free(target_adapters); return json_null(); }
                pconfig = pc;
            }
        }
    } else if (!sd2_truthy(pconfig)) {
        json_t *out = json_object();
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", sd2_fmt("platform '%s' not configured/enabled", sd2_sval(platform_name)));
        json_free(transport); json_free(target_adapters);
        return out;
    } else {
        json_t *en = json_obj_get(pconfig, "enabled");
        if (!en || en->type != JSON_BOOL || !en->bool_val) {
            json_t *out = json_object();
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", sd2_fmt("platform '%s' not configured/enabled", sd2_sval(platform_name)));
            json_free(transport); json_free(target_adapters);
            return out;
        }
    }
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "error", json_null());
    json_set(out, "transport", sd2_copy_or_null(tnonnull ? transport : NULL));
    json_set(out, "pconfig", sd2_copy_or_null(pconfig));
    json_set(out, "runtime_adapter", sd2_copy_or_null(runtime));
    json_set(out, "target_adapters", json_copy(target_adapters));
    json_free(transport); json_free(target_adapters);
    return out;
}

/* 21. _named_dm_topic */
/* PoP: sd2_named_dm_topic @ cron/scheduler_delivery.py:_named_dm_topic */
static json_t *sd2_named_dm_topic(sd2_io_t *io, json_t *t) {
    const char *plat = sd2_sval(json_obj_get(t, "platform"));
    json_t *tid = json_obj_get(t, "thread_id");
    if (strcmp(plat, "telegram") == 0 && tid && tid->type != JSON_NULL) {
        json_t *a = sd2_args(); json_set(a, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
        json_t *r1 = sd2_seam(io, "looks_like_telegram_private_chat_id", a);
        if (sd2_is_raise(r1)) return r1;
        if (sd2_truthy(r1)) {
            json_t *a2 = sd2_args(); json_set(a2, "thread_id", sd2_copy_or_null(tid));
            json_t *r2 = sd2_seam(io, "_looks_like_int", a2);
            if (sd2_is_raise(r2)) { json_free(r1); return r2; }
            bool is_int = sd2_truthy(r2);
            json_free(r2); json_free(r1);
            if (!is_int) return sd2_pystr(tid);
        } else json_free(r1);
    }
    return json_null();
}

/* 22. _live_route_metadata (echoes via ret dict) */
/* PoP: sd2_live_route_metadata @ cron/scheduler_delivery.py:_live_route_metadata */
static json_t *sd2_live_route_metadata(sd2_io_t *io, json_t *t) {
    json_t *job = json_obj_get(t, "job");
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    json_t *tid = json_obj_get(t, "thread_id");
    const char *plat = sd2_sval(json_obj_get(t, "platform"));
    bool tid_nonnull = tid && tid->type != JSON_NULL;
    bool ambiguous = false;
    if (strcmp(plat, "telegram") == 0 && tid_nonnull) {
        json_t *a = sd2_args(); json_set(a, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
        json_t *r1 = sd2_seam(io, "looks_like_telegram_private_chat_id", a);
        if (sd2_is_raise(r1)) return r1;
        if (sd2_truthy(r1)) {
            json_t *a2 = sd2_args(); json_set(a2, "thread_id", sd2_copy_or_null(tid));
            json_t *r2 = sd2_seam(io, "_looks_like_int", a2);
            if (sd2_is_raise(r2)) { json_free(r1); return r2; }
            ambiguous = sd2_truthy(r2);
            json_free(r2);
        }
        json_free(r1);
    }
    json_t *route_metadata = json_object();
    json_t *media_metadata = json_object();
    json_t *route_thread_id = NULL;
    if (ambiguous) {
        json_t *a = sd2_args();
        json_set(a, "adapter", sd2_copy_or_null(json_obj_get(t, "runtime_adapter")));
        json_set(a, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
        json_set(a, "loop", sd2_copy_or_null(json_obj_get(t, "loop")));
        json_set(a, "job_id", json_string(jid));
        json_t *cd = sd2_seam(io, "is_channel_dm_topic", a);
        if (sd2_is_raise(cd)) return cd;
        bool is_cd = sd2_truthy(cd); json_free(cd);
        if (is_cd) {
            json_t *ts = sd2_pystr(tid);
            json_set(route_metadata, "direct_messages_topic_id", sd2_copy_or_null(ts));
            json_set(route_metadata, "job_id", json_string(jid));
            json_set(route_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
            json_set(media_metadata, "direct_messages_topic_id", sd2_copy_or_null(ts));
            json_set(media_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
            json_free(ts);
            route_thread_id = json_null();
        } else {
            route_thread_id = tid_nonnull ? sd2_pystr(tid) : json_null();
            json_set(route_metadata, "job_id", json_string(jid));
            json_set(route_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
            bool none_topic = true;
            if (sd2_truthy(route_thread_id)) {
                json_t *nm = sd2_seam(io, "named_dm_topic", sd2_args());
                if (sd2_is_raise(nm)) { json_free(route_thread_id); return nm; }
                none_topic = (nm && nm->type == JSON_NULL) || !nm;
                json_free(nm);
                if (none_topic)
                    json_set(route_metadata, "thread_id", sd2_copy_or_null(route_thread_id));
            }
            json_set(media_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
            if (sd2_truthy(tid)) json_set(media_metadata, "thread_id", sd2_copy_or_null(tid));
        }
    } else {
        route_thread_id = tid_nonnull ? sd2_pystr(tid) : json_null();
        json_set(route_metadata, "job_id", json_string(jid));
        json_set(route_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
        if (sd2_truthy(route_thread_id)) {
            json_t *nm = sd2_seam(io, "named_dm_topic", sd2_args());
            if (sd2_is_raise(nm)) { json_free(route_thread_id); return nm; }
            bool none_topic = (nm && nm->type == JSON_NULL) || !nm;
            json_free(nm);
            if (none_topic)
                json_set(route_metadata, "thread_id", sd2_copy_or_null(route_thread_id));
        }
        json_set(media_metadata, "notify", sd2_copy_or_null(json_obj_get(t, "notify_delivery")));
        if (sd2_truthy(tid)) json_set(media_metadata, "thread_id", sd2_copy_or_null(tid));
    }
    /* _origin.stamp_origin_discriminators(t, route_metadata, media_metadata) */
    json_t *st = sd2_seam(io, "stamp_origin_discriminators", sd2_args());
    if (sd2_is_raise(st)) { json_free(route_thread_id); json_free(route_metadata); json_free(media_metadata); return st; }
    if (st && st->type == JSON_OBJECT) {
        json_t *rm = json_obj_get(st, "route_metadata");
        if (rm && rm->type == JSON_OBJECT)
            for (size_t i = 0; i < rm->c.count; i++)
                json_set(route_metadata, rm->c.keys[i], json_copy(rm->c.items[i]));
        json_t *mm = json_obj_get(st, "media_metadata");
        if (mm && mm->type == JSON_OBJECT)
            for (size_t i = 0; i < mm->c.count; i++)
                json_set(media_metadata, mm->c.keys[i], json_copy(mm->c.items[i]));
    }
    json_free(st);
    json_t *out = json_object();
    json_set(out, "route_thread_id", sd2_copy_or_null(route_thread_id));
    json_set(out, "route_metadata", route_metadata);
    json_set(out, "media_metadata", media_metadata);
    json_free(route_thread_id);
    return out;
}


/* t helpers */
static bool sd2_t_is_relay(const json_t *t) {
    json_t *tr = json_obj_get(t, "transport");
    if (!tr || tr->type == JSON_NULL) return false;
    json_t *r = json_obj_get(tr, "is_relay");
    return r && r->type == JSON_BOOL && r->bool_val;
}
static json_t *sd2_t_where(const json_t *t) {
    const char *pn = sd2_sval(json_obj_get(t, "platform_name"));
    const char *cid = sd2_sval(json_obj_get(t, "chat_id"));
    json_t *tid = json_obj_get(t, "thread_id");
    if (tid && tid->type != JSON_NULL && sd2_truthy(tid))
        return sd2_fmt("%s:%s:%s", pn, cid, sd2_sval(tid));
    return sd2_fmt("%s:%s", pn, cid);
}
static void sd2_append_str(json_t *arr, const char *s) {
    if (arr && arr->type == JSON_ARRAY) json_append(arr, json_string(s ? s : ""));
}
static void sd2_extend(json_t *dst, json_t *src) {
    if (!dst || !src || dst->type != JSON_ARRAY || src->type != JSON_ARRAY) return;
    for (size_t i = 0; i < src->c.count; i++) json_append(dst, json_copy(json_get(src, i)));
}
static bool sd2_contains_sub(json_t *arr, const char *sub) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *s = sd2_sval(json_get(arr, i));
        if (s && strstr(s, sub)) return true;
    }
    return false;
}

/* 23. _live_send_text -> [ok, timed_out, msg_id] ; echoes errors + t */
static json_t *sd2_live_send_ret(bool ok, bool timed_out, json_t *msgid) {
    json_t *a = json_array();
    json_append(a, json_bool(ok));
    json_append(a, json_bool(timed_out));
    json_append(a, sd2_copy_or_null(msgid));
    return a;
}
/* PoP: sd2_live_send_text @ cron/scheduler_delivery.py:_live_send_text */
static json_t *sd2_live_send_text(sd2_io_t *io, json_t *t, json_t *text,
        json_t *route_thread_id, json_t *route_metadata,
        json_t *target_errors, json_t *delivery_errors, json_t *unverified_targets) {
    (void)text; (void)route_metadata; (void)delivery_errors;
    json_t *job = json_obj_get(t, "job");
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    json_t *pending = NULL;
    json_t *a = sd2_args(); json_set(a, "config", sd2_copy_or_null(json_obj_get(t, "config")));
    json_t *router = sd2_seam(io, "DeliveryRouter", a);
    if (sd2_is_raise(router)) return router;
    json_free(router);
    json_t *a2 = sd2_args();
    json_set(a2, "platform", sd2_copy_or_null(json_obj_get(t, "platform")));
    json_set(a2, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
    json_set(a2, "thread_id", sd2_copy_or_null(route_thread_id));
    json_t *rt = sd2_seam(io, "DeliveryTarget", a2);
    if (sd2_is_raise(rt)) return rt;
    json_free(rt);
    json_t *a3 = sd2_args(); json_set(a3, "loop", sd2_copy_or_null(json_obj_get(t, "loop")));
    json_t *future = sd2_seam(io, "safe_schedule_threadsafe", a3);
    if (sd2_is_raise(future)) return future;
    bool future_none = !future || future->type == JSON_NULL;
    if (future_none) {
        sd2_append_str(target_errors, "live adapter event loop scheduling failed");
        json_free(future);
        return sd2_live_send_ret(false, false, NULL);
    }
    json_t *a4 = sd2_args(); json_set(a4, "future", sd2_copy_or_null(future));
    json_t *sr = sd2_seam(io, "future.result", a4);
    json_free(future);
    if (sd2_is_raise(sr)) {
        pending = sr;
        const char *kind = sd2_exc_kind(pending);
        const char *msg = sd2_exc_msg(pending);
        json_t *wh = sd2_t_where(t);
        const char *ws = sd2_sval(wh);
        if (sd2_is_subclass(kind, "TimeoutError")) {
            bool started = false;
            json_t *stf = json_obj_get(pending, "started");
            if (stf && stf->type == JSON_BOOL && stf->bool_val) started = true;
            char buf[1024];
            if (!started) {
                snprintf(buf, sizeof buf, "live adapter send to %s timed out before the coroutine was dispatched", ws);
                json_t *w = sd2_args(); json_set(w, "msg", json_string(buf));
                json_free(sd2_seam(io, "logger.warning", w));
                sd2_append_str(target_errors, buf);
                json_free(wh); json_free(pending);
                return sd2_live_send_ret(false, false, NULL);
            }
            json_free(sd2_seam(io, "logger.warning", sd2_args()));
            json_free(wh); json_free(pending);
            return sd2_live_send_ret(true, true, NULL);
        }
        if (strcmp(kind, "PartialDeliveryError") == 0) {
            json_t *res = json_obj_get(pending, "result");
            json_t *raw = res ? json_obj_get(res, "raw_response") : NULL;
            const char *dc = json_get_str(raw, "delivered_chunks", "?");
            const char *tc = json_get_str(raw, "total_chunks", "?");
            char buf[1024];
            snprintf(buf, sizeof buf, "live adapter send to %s delivered %s of %s chunks, then failed: %s",
                     ws, dc ? dc : "?", tc ? tc : "?", msg ? msg : "");
            json_t *a5 = sd2_args(); json_set(a5, "job_id", json_string(jid)); json_set(a5, "msg", json_string(buf));
            json_free(sd2_seam(io, "note_target_error", a5));
            sd2_append_str(delivery_errors, buf);
            json_free(wh); json_free(pending);
            return sd2_live_send_ret(true, false, NULL);
        }
        if (sd2_caught(kind)) {
            json_set(t, "live_error", json_string(msg ? msg : ""));
            json_t *retry = json_obj_get(pending, "send_retryable");
            json_set(t, "live_retryable", sd2_copy_or_null(retry));
            char buf[1024];
            snprintf(buf, sizeof buf, "live adapter send failed: %s", msg ? msg : "");
            sd2_append_str(target_errors, buf);
            json_free(wh);
            return pending;
        }
        json_free(wh);
        return pending;
    }
    /* success path */
    json_t *a6 = sd2_args(); json_set(a6, "result", sd2_copy_or_null(sr)); json_set(a6, "key", json_string("raw_response"));
    json_t *rawresp = sd2_seam(io, "result_field", a6);
    json_t *a7 = sd2_args(); json_set(a7, "result", sd2_copy_or_null(sr)); json_set(a7, "key", json_string("message_id"));
    json_t *msgid = sd2_seam(io, "result_field", a7);
    json_t *a8 = sd2_args(); json_set(a8, "result", sd2_copy_or_null(sr)); json_set(a8, "job_id", json_string(jid));
    json_t *cf = sd2_seam(io, "confirm_adapter_delivery", a8);
    if (sd2_is_raise(cf)) { json_free(sr); json_free(rawresp); json_free(msgid); return cf; }
    bool send_success = false;
    json_t *evarr = NULL;
    if (cf && cf->type == JSON_OBJECT) {
        send_success = sd2_truthy(json_obj_get(cf, "ok"));
        evarr = json_obj_get(cf, "evidence");
    }
    json_free(cf);
    if (send_success && evarr && evarr->type == JSON_ARRAY && evarr->c.count > 0) {
        json_t *wh = sd2_t_where(t);
        sd2_append_str(unverified_targets, sd2_sval(wh));
        json_free(wh);
    }
    if (!send_success) {
        const char *err = NULL; const char *shape = "None";
        if (!sr || sr->type == JSON_NULL) { err = "no response from adapter"; }
        else if (sr->type == JSON_OBJECT) {
            err = json_get_str(sr, "error", "");
            if (!err || !err[0]) err = json_get_str(sr, "filtered", "");
            if (!err || !err[0]) err = "unknown";
            shape = "dict";
        } else { err = json_get_str(sr, "error", ""); shape = "object"; }
        char buf[1024];
        snprintf(buf, sizeof buf, "live adapter send to %s returned unconfirmed result (%s, error=%s)",
                 sd2_sval(sd2_t_where(t)), shape, err ? err : "None");
        json_set(t, "live_error", (err && err[0]) ? json_string(err) : json_null());
        json_t *retry = (sr && sr->type == JSON_OBJECT) ? json_obj_get(sr, "retryable") : NULL;
        json_set(t, "live_retryable", sd2_copy_or_null(retry));
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "msg", json_string(buf)); json_set(w, "is_relay", json_bool(sd2_t_is_relay(t)));
        json_free(sd2_seam(io, "warn_live_lane_failure", w));
        sd2_append_str(target_errors, buf);
        json_free(sr); json_free(rawresp); json_free(msgid);
        return sd2_live_send_ret(false, false, NULL);
    }
    bool thread_fallback = false;
    const char *req_tid = NULL;
    if (rawresp && rawresp->type == JSON_OBJECT && sd2_truthy(json_obj_get(t, "thread_id"))) {
        if (sd2_truthy(json_obj_get(rawresp, "thread_fallback"))) {
            thread_fallback = true;
            req_tid = json_get_str(rawresp, "requested_thread_id", "");
            if (!req_tid || !req_tid[0]) req_tid = sd2_sval(json_obj_get(t, "thread_id"));
        }
    }
    if (thread_fallback) {
        char buf[1024];
        snprintf(buf, sizeof buf, "configured thread_id %s for %s was not found; delivered without thread_id",
                 req_tid ? req_tid : "", sd2_sval(sd2_t_where(t)));
        json_t *a9 = sd2_args(); json_set(a9, "job_id", json_string(jid)); json_set(a9, "msg", json_string(buf));
        json_free(sd2_seam(io, "note_target_error", a9));
        sd2_append_str(delivery_errors, buf);
    }
    json_t *out = sd2_live_send_ret(true, false, msgid);
    json_free(sr); json_free(rawresp); json_free(msgid);
    return out;
}

/* 24. _resolve_named_dm_topic */
/* PoP: sd2_resolve_named_dm_topic @ cron/scheduler_delivery.py:_resolve_named_dm_topic */
static json_t *sd2_resolve_named_dm_topic(sd2_io_t *io, json_t *t, json_t *topic_name,
        json_t *metadata, json_t *delivery_errors, long long n_media, bool refresh) {
    const char *jid = sd2_sval(json_obj_get(json_obj_get(t, "job"), "id"));
    json_t *a1 = sd2_args();
    json_set(a1, "adapter", sd2_copy_or_null(json_obj_get(t, "runtime_adapter")));
    json_set(a1, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
    json_set(a1, "topic", sd2_copy_or_null(topic_name));
    json_set(a1, "refresh", json_bool(refresh));
    json_free(sd2_seam(io, "_ensure_named_dm_topic", a1));
    json_t *a2 = sd2_args(); json_set(a2, "loop", sd2_copy_or_null(json_obj_get(t, "loop")));
    json_t *future = sd2_seam(io, "safe_schedule_threadsafe", a2);
    bool fail = false;
    json_t *exc = NULL;
    if (!future || future->type == JSON_NULL) {
        exc = sd2_raise("RuntimeError", "gateway loop unavailable"); fail = true;
    } else {
        json_t *a3 = sd2_args(); json_set(a3, "future", sd2_copy_or_null(future));
        json_t *r = sd2_seam(io, "future.result", a3);
        json_free(future);
        if (sd2_is_raise(r)) { exc = r; fail = true; }
        else { json_set(t, "thread_id", sd2_copy_or_null(r)); json_free(r); }
    }
    if (fail) {
        if (exc && !sd2_caught(sd2_exc_kind(exc))) return exc;
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "topic", sd2_copy_or_null(topic_name));
        json_free(sd2_seam(io, "logger.warning", w));
        if (n_media) {
            char buf[1024];
            snprintf(buf, sizeof buf, "%lld media attachment(s) not delivered to %s: %s",
                     n_media, sd2_sval(sd2_t_where(t)), sd2_exc_msg(exc));
            sd2_append_str(delivery_errors, buf);
        }
        if (exc) json_free(exc);
        return json_bool(false);
    }
    json_set(metadata, "thread_id", sd2_copy_or_null(json_obj_get(t, "thread_id")));
    json_set(metadata, "telegram_dm_topic_created_for_send", json_bool(true));
    return json_bool(true);
}

/* 25. _live_send_media */
/* PoP: sd2_live_send_media @ cron/scheduler_delivery.py:_live_send_media */
static json_t *sd2_live_send_media(sd2_io_t *io, json_t *t, json_t *media_metadata,
        json_t *media_files, json_t *delivery_errors, json_t *unverified_targets, json_t *topic_name) {
    json_t *routed = media_metadata && media_metadata->type == JSON_OBJECT
                     ? json_copy(media_metadata) : json_object();
    if (sd2_t_is_relay(t)) {
        json_set(routed, "_relay_logical_platform", sd2_copy_or_null(json_obj_get(t, "platform")));
        json_t *a = sd2_args();
        json_set(a, "platform", sd2_copy_or_null(json_obj_get(t, "platform")));
        json_t *lh = sd2_seam(io, "get_home_channel", a);
        if (sd2_is_raise(lh)) { json_free(routed); return lh; }
        if (lh && lh->type == JSON_OBJECT) {
            json_t *chat = json_obj_get(lh, "chat_id");
            if (chat && sd2_sval(chat) &&
                strcmp(sd2_sval(chat), sd2_sval(json_obj_get(t, "chat_id"))) == 0) {
                json_t *u = json_obj_get(lh, "user_id");
                if (sd2_truthy(u)) json_set(routed, "user_id", sd2_copy_or_null(u));
                json_t *s = json_obj_get(lh, "scope_id");
                if (sd2_truthy(s)) json_set(routed, "scope_id", sd2_copy_or_null(s));
            }
        }
        json_free(lh);
    }
    json_t *a1 = sd2_args();
    json_set(a1, "adapter", sd2_copy_or_null(json_obj_get(t, "runtime_adapter")));
    json_set(a1, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
    json_set(a1, "files", sd2_copy_or_null(media_files));
    json_set(a1, "metadata", json_copy(routed));
    json_set(a1, "force_document", sd2_copy_or_null(json_obj_get(t, "force_document")));
    json_t *errs = sd2_seam(io, "send_media_via_adapter", a1);
    if (sd2_is_raise(errs)) { json_free(routed); return errs; }
    long long nfiles = (media_files && media_files->type == JSON_ARRAY) ? (long long)media_files->c.count : 0;
    long long notfound = 0;
    if (errs && errs->type == JSON_ARRAY)
        for (size_t i = 0; i < errs->c.count; i++) {
            const char *e = sd2_sval(json_get(errs, i));
            if (e) { char low[512]; sd2_lower_copy(e, low, sizeof low); if (strstr(low, "thread not found")) notfound++; }
        }
    if (sd2_truthy(topic_name) && nfiles == notfound) {
        json_t *ra = sd2_args();
        json_set(ra, "topic", sd2_copy_or_null(topic_name));
        json_set(ra, "refresh", json_bool(true));
        json_set(ra, "n_media", json_number((double)nfiles));
        json_t *r = sd2_seam(io, "resolve_named_dm_topic", ra);
        if (sd2_is_raise(r)) { json_free(errs); json_free(routed); return r; }
        if (sd2_truthy(r)) {
            json_free(errs);
            json_t *a2 = sd2_args();
            json_set(a2, "adapter", sd2_copy_or_null(json_obj_get(t, "runtime_adapter")));
            json_set(a2, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
            json_set(a2, "files", sd2_copy_or_null(media_files));
            json_set(a2, "metadata", json_copy(routed));
            json_set(a2, "force_document", sd2_copy_or_null(json_obj_get(t, "force_document")));
            errs = sd2_seam(io, "send_media_via_adapter", a2);
        } else json_free(r);
    }
    json_t *job = json_obj_get(t, "job");
    json_t *lost = json_obj_get(job, "_media_ack_lost");
    bool pop_lost = sd2_truthy(lost);
    if (pop_lost) json_obj_del(job, "_media_ack_lost");
    if (pop_lost) {
        json_t *wh = sd2_t_where(t);
        const char *ws = sd2_sval(wh);
        bool seen = false;
        if (unverified_targets && unverified_targets->type == JSON_ARRAY)
            for (size_t i = 0; i < unverified_targets->c.count; i++)
                if (strcmp(sd2_sval(json_get(unverified_targets, i)), ws) == 0) { seen = true; break; }
        if (!seen) sd2_append_str(unverified_targets, ws);
        json_free(wh);
    }
    if (errs && errs->type == JSON_ARRAY) {
        char buf[1024];
        for (size_t i = 0; i < errs->c.count; i++) {
            snprintf(buf, sizeof buf, "%s (target %s)", sd2_sval(json_get(errs, i)),
                     sd2_sval(sd2_t_where(t)));
            sd2_append_str(delivery_errors, buf);
        }
    }
    json_free(errs); json_free(routed);
    return json_null();
}


/* 26. _seed_live_delivery_sessions */
/* PoP: sd2_seed_live_delivery_sessions @ cron/scheduler_delivery.py:_seed_live_delivery_sessions */
static json_t *sd2_seed_live_delivery_sessions(sd2_io_t *io, json_t *t, json_t *delivered_message_id) {
    json_t *job = json_obj_get(t, "job");
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    json_t *origin = json_obj_get(t, "origin");
    if (!origin || origin->type != JSON_OBJECT) origin = json_object();
    bool thread_seeded = false, inchannel_seeded = false;
    if (sd2_truthy(json_obj_get(t, "opened_thread_id"))) {
        json_t *a = sd2_args();
        json_set(a, "job", json_copy(job));
        json_set(a, "thread", sd2_copy_or_null(json_obj_get(t, "opened_thread_id")));
        json_t *r = sd2_seam(io, "seed_cron_thread_session", a);
        if (sd2_is_raise(r)) return r;
        json_free(r);
        thread_seeded = true;
    }
    if (sd2_truthy(json_obj_get(t, "in_channel_surface")) &&
        sd2_truthy(json_obj_get(t, "inchannel_continuable")) && !thread_seeded) {
        json_t *a = sd2_args();
        json_set(a, "job", json_copy(job));
        json_t *r = sd2_seam(io, "seed_cron_channel_session", a);
        if (sd2_is_raise(r)) return r;
        inchannel_seeded = sd2_truthy(r);
        json_free(r);
        if (!inchannel_seeded) {
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
            json_set(w, "platform", sd2_copy_or_null(json_obj_get(t, "platform_name")));
            json_set(w, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
            json_free(sd2_seam(io, "logger.warning", w));
        }
        if (sd2_truthy(delivered_message_id)) {
            json_t *a2 = sd2_args();
            json_set(a2, "job", json_copy(job));
            json_set(a2, "thread", sd2_copy_or_null(delivered_message_id));
            json_t *r2 = sd2_seam(io, "seed_cron_thread_session", a2);
            if (sd2_is_raise(r2)) return r2;
            json_free(r2);
        }
    } else if (sd2_truthy(json_obj_get(t, "in_channel_surface")) &&
               !sd2_truthy(json_obj_get(t, "inchannel_continuable"))) {
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "platform", sd2_copy_or_null(json_obj_get(t, "platform_name")));
        json_set(w, "chat_id", sd2_copy_or_null(json_obj_get(t, "chat_id")));
        json_set(w, "origin_platform", sd2_copy_or_null(json_obj_get(origin, "platform")));
        json_set(w, "origin_chat", sd2_copy_or_null(json_obj_get(origin, "chat_id")));
        json_set(w, "origin_thread", sd2_copy_or_null(json_obj_get(origin, "thread_id")));
        json_free(sd2_seam(io, "logger.warning", w));
    }
    json_t *m = sd2_args();
    json_set(m, "job", json_copy(job));
    json_set(m, "enabled", json_bool(sd2_truthy(json_obj_get(t, "mirror_this_target"))
                                     && !thread_seeded && !inchannel_seeded));
    json_t *r = sd2_seam(io, "maybe_mirror_cron_delivery", m);
    if (sd2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* 27. _deliver_via_live_adapter -> bool ; echoes errors arrays + t */
/* PoP: sd2_deliver_via_live_adapter @ cron/scheduler_delivery.py:_deliver_via_live_adapter */
static json_t *sd2_deliver_via_live_adapter(sd2_io_t *io, json_t *t, json_t *cleaned_text,
        json_t *media_files, json_t *target_errors, json_t *delivery_errors,
        json_t *unverified_targets) {
    json_t *job = json_obj_get(t, "job");
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    json_t *pending = NULL;
    json_t *r = sd2_seam(io, "live_route_metadata", sd2_args());
    if (sd2_is_raise(r)) return r;
    json_t *tid = json_obj_get(r, "route_thread_id");
    json_t *route_metadata = json_obj_get(r, "route_metadata");
    json_t *media_metadata = json_obj_get(r, "media_metadata");
    bool delivered = false;
    bool adapter_ok = true, timed_out = false;
    json_t *msgid = NULL;
    /* text_to_send = cleaned_text.strip() */
    bool has_text = false;
    if (cleaned_text && cleaned_text->type == JSON_STRING && cleaned_text->str_val) {
        const char *s = cleaned_text->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        has_text = *s != '\0';
    }
    bool has_media = sd2_truthy(media_files);
    if (!has_text && !has_media) {
        char buf[512];
        snprintf(buf, sizeof buf, "live adapter send skipped (empty text and no media) for %s",
                 sd2_sval(sd2_t_where(t)));
        json_t *a = sd2_args(); json_set(a, "job_id", json_string(jid)); json_set(a, "msg", json_string(buf));
        json_free(sd2_seam(io, "note_target_error", a));
        sd2_append_str(target_errors, buf);
        adapter_ok = false;
    } else if (has_text) {
        json_t *a = sd2_args();
        json_set(a, "thread_id", sd2_copy_or_null(tid));
        json_set(a, "route_metadata", sd2_copy_or_null(route_metadata));
        json_t *res = sd2_seam(io, "live_send_text", a);
        if (sd2_is_raise(res) && !sd2_caught(sd2_exc_kind(res))) { pending = res; goto dva_except; }
        if (sd2_is_raise(res)) { pending = res; goto dva_except; }
        if (res && res->type == JSON_ARRAY && res->c.count >= 2) {
            adapter_ok = sd2_truthy(json_get(res, 0));
            timed_out = sd2_truthy(json_get(res, 1));
            msgid = json_get(res, 2) ? json_copy(json_get(res, 2)) : NULL;
        }
        json_free(res);
    }
    json_t *topic = sd2_seam(io, "named_dm_topic", sd2_args());
    if (sd2_is_raise(topic)) { pending = topic; goto dva_except; }
    bool topic_nonnull = topic && topic->type != JSON_NULL;
    if (topic_nonnull && adapter_ok && !timed_out) {
        json_t *a = sd2_args(); json_set(a, "topic", sd2_copy_or_null(topic));
        json_set(a, "refresh", json_bool(false));
        json_set(a, "n_media", json_number(has_media ? (double)media_files->c.count : 0.0));
        json_t *res = sd2_seam(io, "resolve_named_dm_topic", a);
        if (sd2_is_raise(res)) { pending = res; goto dva_except; }
        adapter_ok = sd2_truthy(res) || has_text;
        json_free(res);
    }
    json_free(topic);
    if (adapter_ok && !timed_out && has_media) {
        json_t *a = sd2_args();
        json_set(a, "files", sd2_copy_or_null(media_files));
        json_t *res = sd2_seam(io, "live_send_media", a);
        if (sd2_is_raise(res)) { pending = res; goto dva_except; }
        json_free(res);
    } else if (timed_out && has_media) {
        char buf[512];
        snprintf(buf, sizeof buf, "%lld media attachment(s) not delivered to %s (live adapter confirmation timed out)",
                 (long long)media_files->c.count, sd2_sval(sd2_t_where(t)));
        json_t *a = sd2_args(); json_set(a, "job_id", json_string(jid)); json_set(a, "msg", json_string(buf));
        json_free(sd2_seam(io, "note_target_error", a));
        sd2_append_str(delivery_errors, buf);
    }
    if (adapter_ok) {
        json_t *a = sd2_args(); json_set(a, "job_id", json_string(jid));
        json_set(a, "thread", sd2_copy_or_null(tid));
        json_set(a, "msgid", sd2_copy_or_null(msgid));
        json_free(sd2_seam(io, "logger.info", a));
        delivered = true;
        json_t *s = sd2_seam(io, "seed_live_delivery_sessions", sd2_args());
        if (sd2_is_raise(s)) { pending = s; goto dva_except; }
        json_free(s);
    }
    json_free(msgid); json_free(r);
    return json_bool(delivered);
dva_except:
    {
        const char *msg = sd2_exc_msg(pending);
        char buf[1024];
        snprintf(buf, sizeof buf, "live adapter delivery to %s failed: %s",
                 sd2_sval(sd2_t_where(t)), msg ? msg : "");
        if (!sd2_contains_sub(target_errors, buf)) sd2_append_str(target_errors, buf);
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "msg", json_string(buf)); json_set(w, "is_relay", json_bool(sd2_t_is_relay(t)));
        json_free(sd2_seam(io, "warn_live_lane_failure", w));
        if (msgid) json_free(msgid);
        if (r) json_free(r);
        if (pending && !sd2_caught(sd2_exc_kind(pending))) return pending;
        if (pending) json_free(pending);
        return json_bool(false);
    }
}

/* 28. _deliver_standalone -> bool|null ; echoes errors arrays + t */
/* PoP: sd2_deliver_standalone @ cron/scheduler_delivery.py:_deliver_standalone */
static json_t *sd2_deliver_standalone(sd2_io_t *io, json_t *t, json_t *content,
        json_t *media_files, json_t *target_errors, json_t *delivery_errors) {
    (void)media_files;
    json_t *job = json_obj_get(t, "job");
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    if (sd2_t_is_relay(t)) {
        if (!target_errors || target_errors->c.count == 0) {
            char buf[512]; snprintf(buf, sizeof buf, "relay delivery to %s failed", sd2_sval(sd2_t_where(t)));
            sd2_append_str(target_errors, buf);
        }
        sd2_extend(delivery_errors, target_errors);
        return json_null();
    }
    json_t *a = sd2_args(); json_set(a, "content", sd2_copy_or_null(content));
    json_t *sr = sd2_seam(io, "standalone_send", a);
    if (sd2_is_raise(sr)) return sr;
    json_t *result = json_obj_get(sr, "result");
    json_t *err = json_obj_get(sr, "err");
    bool partial = false;
    if ((!err || err->type == JSON_NULL) && result && result->type == JSON_OBJECT) {
        json_t *ej = json_obj_get(result, "error");
        if (sd2_truthy(ej)) {
            char buf[1024];
            snprintf(buf, sizeof buf, "delivery error: %s (target %s)", sd2_sval(ej),
                     sd2_sval(sd2_t_where(t)));
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid)); json_set(w, "msg", json_string(buf));
            json_free(sd2_seam(io, "logger.error", w));
            json_t *a2 = sd2_args(); json_set(a2, "result", sd2_copy_or_null(result));
            json_t *state = sd2_seam(io, "delivery_state", a2);
            if (sd2_is_raise(state)) { json_free(state); json_free(sr); return state; }
            const char *st = sd2_sval(state);
            if (strcmp(st, "none") != 0) json_set(t, "standalone_in_flight", json_bool(true));
            partial = (strcmp(st, "partial") == 0);
            json_free(state);
            err = json_string(buf);
        }
    }
    if (err && err->type != JSON_NULL) {
        sd2_append_str(target_errors, sd2_sval(err));
        sd2_extend(delivery_errors, target_errors);
        json_t *a2 = sd2_args();
        json_set(a2, "content", sd2_copy_or_null(json_obj_get(t, "mirror_text")));
        json_free(sd2_seam(io, "record_cron_delivery", a2));
        if (sd2_truthy(json_obj_get(t, "standalone_in_flight"))) {
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
            json_set(w, "where", sd2_copy_or_null(sd2_t_where(t)));
            json_free(sd2_seam(io, "logger.warning", w));
        } else {
            json_t *q = sd2_args(); json_set(q, "content", sd2_copy_or_null(content));
            json_free(sd2_seam(io, "queue_for_live_reconnect", q));
        }
        bool in_flight = sd2_truthy(json_obj_get(t, "standalone_in_flight"));
        bool uncertain = (!result || result->type == JSON_NULL) ||
                         (result && strcmp(sd2_sval(json_obj_get(result, "delivery")), "uncertain") == 0);
        bool ret = partial || (in_flight && uncertain);
        json_free(sr); if (err != json_obj_get(sr, "err")) json_free(err);
        return json_bool(ret);
    }
    json_t *warns = (result && result->type == JSON_OBJECT) ? json_obj_get(result, "warnings") : NULL;
    if (warns && warns->type == JSON_ARRAY)
        for (size_t i = 0; i < warns->c.count; i++) {
            char buf[1024];
            snprintf(buf, sizeof buf, "delivery warning: %s (target %s)", sd2_sval(json_get(warns, i)),
                     sd2_sval(sd2_t_where(t)));
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid)); json_set(w, "msg", json_string(buf));
            json_free(sd2_seam(io, "logger.error", w));
            sd2_append_str(delivery_errors, buf);
        }
    json_t *i = sd2_args(); json_set(i, "job_id", json_string(jid));
    json_free(sd2_seam(io, "logger.info", i));
    json_t *a3 = sd2_args(); json_set(a3, "content", sd2_copy_or_null(json_obj_get(t, "mirror_text")));
    json_free(sd2_seam(io, "record_cron_delivery", a3));
    json_t *m = sd2_args(); json_set(m, "job", json_copy(job));
    json_set(m, "enabled", json_bool(sd2_truthy(json_obj_get(t, "mirror_this_target"))));
    json_free(sd2_seam(io, "maybe_mirror_cron_delivery", m));
    json_free(sr);
    return json_bool(true);
}

/* 29. _prepare_target_delivery -> target dict or null ; echoes delivery_errors */
/* PoP: sd2_prepare_target_delivery @ cron/scheduler_delivery.py:_prepare_target_delivery */
static json_t *sd2_prepare_target_delivery(sd2_io_t *io, json_t *job, json_t *target,
        json_t *adapters, json_t *loop, json_t *config, bool notify_delivery, bool mirror_enabled,
        json_t *mirror_text, json_t *delivery_errors) {
    const char *jid = sd2_sval(json_obj_get(job, "id"));
    json_t *platform_name = json_obj_get(target, "platform");
    const char *pname = sd2_sval(platform_name);
    json_t *chat_id = json_obj_get(target, "chat_id");
    json_t *thread_id = json_obj_get(target, "thread_id");
    if (!thread_id) thread_id = json_null();
    json_t *r = sd2_seam(io, "resolve_origin", sd2_args());
    json_t *origin = (r && r->type == JSON_OBJECT && !sd2_is_raise(r)) ? r : json_object();
    json_t *origin_thread = json_obj_get(origin, "thread_id");
    if (sd2_truthy(origin_thread) && !sd2_truthy(thread_id)) {
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "origin_thread", sd2_copy_or_null(origin_thread));
        json_set(w, "deliver", sd2_copy_or_null(json_obj_get(job, "deliver")));
        json_set(w, "target", sd2_copy_or_null(target));
        json_free(sd2_seam(io, "logger.warning", w));
    } else if (sd2_truthy(thread_id)) {
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
        json_set(w, "platform", sd2_copy_or_null(platform_name));
        json_set(w, "chat_id", sd2_copy_or_null(chat_id));
        json_set(w, "thread_id", sd2_copy_or_null(thread_id));
        json_free(sd2_seam(io, "logger.debug", w));
    }
    json_t *a1 = sd2_args();
    json_set(a1, "origin", json_copy(origin));
    json_set(a1, "platform", sd2_copy_or_null(platform_name));
    json_set(a1, "chat_id", sd2_copy_or_null(chat_id));
    json_set(a1, "thread_id", sd2_copy_or_null(thread_id));
    json_t *ot = sd2_seam(io, "target_matches_origin", a1);
    if (sd2_is_raise(ot)) { json_free(ot); json_free(r); return json_null(); }
    bool origin_target = sd2_truthy(ot); json_free(ot);
    bool mirror_this_target = false;
    if (mirror_enabled) {
        json_t *a2 = sd2_args();
        json_set(a2, "job", json_copy(job));
        json_set(a2, "target", sd2_copy_or_null(target));
        json_t *me = sd2_seam(io, "target_mirror_eligible", a2);
        if (sd2_is_raise(me)) { json_free(me); json_free(r); return json_null(); }
        mirror_this_target = sd2_truthy(me); json_free(me);
    }
    json_t *origin_user_id = origin_target ? json_obj_get(origin, "user_id") : NULL;
    if (!origin_user_id) origin_user_id = json_null();
    char oct[128];
    json_t *octv = json_obj_get(origin, "chat_type");
    sd2_lower_copy(sd2_sval(octv), oct, sizeof oct);
    bool is_dm_target = (strcmp(oct, "dm") == 0) ||
        (!oct[0] && strncmp(sd2_sval(chat_id), "D", 1) == 0);
    bool inchannel_continuable = origin_target;
    if (!inchannel_continuable && mirror_this_target) {
        json_t *a3 = sd2_args();
        json_set(a3, "is_dm", json_bool(is_dm_target));
        json_set(a3, "user_id", sd2_copy_or_null(origin_user_id));
        json_t *ia = sd2_seam(io, "inchannel_seed_allowed", a3);
        if (sd2_is_raise(ia)) { json_free(ia); json_free(r); return json_null(); }
        inchannel_continuable = sd2_truthy(ia); json_free(ia);
    }
    /* Platform(platform_name.lower()) */
    char lower[256]; sd2_lower_copy(pname, lower, sizeof lower);
    json_t *a4 = sd2_args(); json_set(a4, "name", json_string(lower));
    json_t *platform = sd2_seam(io, "Platform", a4);
    if (sd2_is_raise(platform)) {
        const char *k = sd2_exc_kind(platform);
        if (sd2_exc_is(k, "ValueError", "KeyError", NULL)) {
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
            json_set(w, "msg", sd2_fmt("unknown platform '%s'", pname));
            json_free(sd2_seam(io, "note_target_error", w));
            char ebuf[256]; snprintf(ebuf, sizeof ebuf, "unknown platform '%s'", pname);
            sd2_append_str(delivery_errors, ebuf);
        }
        json_free(platform); json_free(r);
        return json_null();
    }
    json_t *a5 = sd2_args();
    json_set(a5, "job", json_copy(job));
    json_set(a5, "platform", sd2_copy_or_null(platform));
    json_set(a5, "platform_name", sd2_copy_or_null(platform_name));
    json_set(a5, "target", sd2_copy_or_null(target));
    json_set(a5, "adapters", sd2_copy_or_null(adapters));
    json_set(a5, "config", sd2_copy_or_null(config));
    json_t *res = sd2_seam(io, "resolve_target_transport", a5);
    if (sd2_is_raise(res)) { json_free(res); json_free(platform); json_free(r); return json_null(); }
    bool ok = sd2_truthy(json_obj_get(res, "ok"));
    if (!ok) {
        json_t *msg = json_obj_get(res, "error");
        const char *ms = sd2_sval(msg);
        json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid)); json_set(w, "msg", sd2_copy_or_null(msg));
        json_free(sd2_seam(io, "note_target_error", w));
        sd2_append_str(delivery_errors, ms);
        json_free(res); json_free(platform); json_free(r);
        return json_null();
    }
    json_t *transport = sd2_copy_or_null(json_obj_get(res, "transport"));
    json_t *pconfig = sd2_copy_or_null(json_obj_get(res, "pconfig"));
    json_t *runtime_adapter = sd2_copy_or_null(json_obj_get(res, "runtime_adapter"));
    json_t *target_adapters = sd2_copy_or_null(json_obj_get(res, "target_adapters"));
    json_free(res);
    bool live_adapter_ready = false;
    if (sd2_truthy(runtime_adapter) && sd2_truthy(loop)) {
        json_t *lr = sd2_seam(io, "loop_is_running", sd2_args());
        if (sd2_is_raise(lr)) { json_free(lr); json_free(platform); json_free(r); return json_null(); }
        live_adapter_ready = sd2_truthy(lr); json_free(lr);
    }
    json_t *sm = sd2_seam(io, "resolve_cron_surface_mode", sd2_args());
    if (sd2_is_raise(sm)) { json_free(sm); json_free(platform); json_free(r); return json_null(); }
    bool in_channel_surface = strcmp(sd2_sval(sm), "in_channel") == 0; json_free(sm);
    if (in_channel_surface && sd2_truthy(runtime_adapter)) {
        json_t *su = sd2_seam(io, "inchannel_surface_supported", sd2_args());
        if (sd2_is_raise(su)) { json_free(su); json_free(platform); json_free(r); return json_null(); }
        if (!sd2_truthy(su)) {
            json_t *w = sd2_args(); json_set(w, "job_id", json_string(jid));
            json_set(w, "platform", sd2_copy_or_null(platform_name));
            json_free(sd2_seam(io, "logger.debug", w));
            in_channel_surface = false;
        }
        json_free(su);
    }
    if (in_channel_surface && inchannel_continuable && live_adapter_ready)
        thread_id = json_null();
    json_t *opened_thread_id = NULL;
    bool cond_origin = false;
    if (mirror_this_target && !in_channel_surface && sd2_truthy(runtime_adapter) && sd2_truthy(loop)) {
        bool tid_empty = !sd2_truthy(thread_id);
        if (!tid_empty) {
            bool oth = false;
            if (origin_target) {
                json_t *o1 = sd2_args(); json_set(o1, "thread_id", sd2_copy_or_null(thread_id));
                json_t *r1 = sd2_seam(io, "origin_thread_is", o1);
                if (sd2_is_raise(r1)) { json_free(r1); json_free(platform); json_free(r); return json_null(); }
                oth = sd2_truthy(r1); json_free(r1);
                if (oth) {
                    json_t *r2 = sd2_seam(io, "fresh_thread_overrides_origin_topic", sd2_args());
                    if (sd2_is_raise(r2)) { json_free(r2); json_free(platform); json_free(r); return json_null(); }
                    cond_origin = sd2_truthy(r2); json_free(r2);
                }
            }
        }
        if (tid_empty || cond_origin) {
            json_t *o = sd2_seam(io, "open_continuable_cron_thread", sd2_args());
            if (sd2_is_raise(o)) { json_free(o); json_free(platform); json_free(r); return json_null(); }
            if (sd2_truthy(o)) { opened_thread_id = o; thread_id = json_copy(o); }
            else json_free(o);
        }
    }
    if (!opened_thread_id) opened_thread_id = json_null();
    json_t *out = json_object();
    json_set(out, "job", json_copy(job));
    json_set(out, "platform", sd2_copy_or_null(platform));
    json_set(out, "platform_name", sd2_copy_or_null(platform_name));
    json_set(out, "chat_id", sd2_copy_or_null(chat_id));
    json_set(out, "thread_id", sd2_copy_or_null(thread_id));
    json_set(out, "transport", sd2_copy_or_null(transport));
    json_set(out, "pconfig", sd2_copy_or_null(pconfig));
    json_set(out, "runtime_adapter", sd2_copy_or_null(runtime_adapter));
    json_set(out, "target_adapters", sd2_copy_or_null(target_adapters));
    json_set(out, "config", sd2_copy_or_null(config));
    json_set(out, "loop", sd2_copy_or_null(loop));
    json_set(out, "notify_delivery", json_bool(notify_delivery));
    json_set(out, "origin", json_copy(origin));
    json_set(out, "origin_target", json_bool(origin_target));
    json_set(out, "origin_user_id", sd2_copy_or_null(origin_user_id));
    json_set(out, "is_dm_target", json_bool(is_dm_target));
    json_set(out, "mirror_text", sd2_copy_or_null(mirror_text));
    json_set(out, "mirror_this_target", json_bool(mirror_this_target));
    json_set(out, "in_channel_surface", json_bool(in_channel_surface));
    json_set(out, "inchannel_continuable", json_bool(inchannel_continuable));
    json_set(out, "opened_thread_id", sd2_copy_or_null(opened_thread_id));
    json_set(out, "live_adapter_ready", json_bool(live_adapter_ready));
    json_set(out, "force_document", json_bool(false));
    json_set(out, "live_error", json_null());
    json_set(out, "live_retryable", json_null());
    json_set(out, "standalone_in_flight", json_bool(false));
    json_free(opened_thread_id); json_free(transport); json_free(pconfig);
    json_free(runtime_adapter); json_free(target_adapters);
    json_free(platform); json_free(r);
    return out;
}

/* 30. _unresolved_delivery_outcome */
/* PoP: sd2_unresolved_delivery_outcome @ cron/scheduler_delivery.py:_unresolved_delivery_outcome */
static json_t *sd2_unresolved_delivery_outcome(sd2_io_t *io, json_t *job, bool for_failure) {
    json_t *a1 = sd2_args(); json_set(a1, "job", json_copy(job)); json_set(a1, "for_failure", json_bool(for_failure));
    json_t *lane = sd2_seam(io, "delivery_lane_value", a1);
    if (sd2_is_raise(lane)) return lane;
    json_t *a2 = sd2_args(); json_set(a2, "value", sd2_copy_or_null(lane));
    json_t *dv = sd2_seam(io, "normalize_deliver_value", a2);
    if (sd2_is_raise(dv)) { json_free(lane); return dv; }
    const char *v = sd2_sval(dv);
    if (strcmp(v, "local") == 0) { json_free(lane); json_free(dv); return json_null(); }
    if (strcmp(v, "origin") == 0) {
        json_t *w = sd2_args();
        json_set(w, "name", sd2_copy_or_null(json_obj_get(job, "name")));
        json_set(w, "id", sd2_copy_or_null(json_obj_get(job, "id")));
        json_free(sd2_seam(io, "logger.info", w));
        json_free(lane); json_free(dv); return json_null();
    }
    char buf[512]; snprintf(buf, sizeof buf, "no delivery target resolved for deliver=%s", v);
    json_t *w = sd2_args(); json_set(w, "job_id", sd2_copy_or_null(json_obj_get(job, "id")));
    json_set(w, "msg", json_string(buf));
    json_free(sd2_seam(io, "logger.warning", w));
    json_free(lane); json_free(dv);
    return json_string(buf);
}
