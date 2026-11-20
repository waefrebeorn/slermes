/*
 * run_inbound_flow3.c — UNCLAIMED remainder FLOW of gateway/run_inbound.py
 * (hermes-agent), third wave after gateway_inbound_flow.c / run_inbound_flow.c
 * (ingress, busy-path, clarify-release cores) and gateway_inbound_flow2.c
 * (command dispatch / admission) claimed their methods. 18 methods of
 * GatewayInboundMixin (pairing / ignored-DM, update-response write, clarify
 * reply, evict-idle / stop-owner-turn, running-session fast path, and the
 * media / text-prep cluster):
 *
 *   _hm_offer_pairing_code, _hm_report_ignored_dm, _hm_write_update_response,
 *   _hm_clarify_reply, _hm_evict_idle_stale_agent, _hm_stop_owner_turn,
 *   _hm_handle_running_session_message, _prefix_inbound_sender_context,
 *   _classify_inbound_media, _enrich_inbound_images, _echo_stt_transcripts,
 *   _inbound_attachment_display_name, _prepend_inbound_media_file_notes,
 *   _prepend_inbound_document_notes, _prepend_inbound_reply_context,
 *   _inbound_model_context_length, _expand_inbound_context_references,
 *   _prepare_clarify_reply_text.
 *
 * Decision sequences only: every live I/O (pairing store, adapters, authority
 * admission rows, activity summaries, config loads, model metadata, vision
 * enrichment, filesystem writes, i18n, clocks) enters through ONE universal
 * seam: ri3_call(ctx, name, args). The harness feeds it the case's scripted
 * results (in.io); the oracle installs recording stubs emitting the identical
 * event shape {"name","args","ret"} from the SAME scripts. Claimed siblings
 * (flow1/flow2 ports) and unported collaborators are SEAMED as "self.<name>";
 * ported siblings compose directly (C calls C, live class methods compose):
 *   _hm_clarify_reply -> _prepare_clarify_reply_text
 *   _prepend_inbound_media_file_notes / _prepend_inbound_document_notes
 *       -> _inbound_attachment_display_name
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ri3_-prefixed.
 *
 * Representation contract (shared with tests/oracle_run_inbound_flow3.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc), ...}; OSError
 *   kinds add errno when set; AuthorityRefusal adds reason/field/detail.
 * - Platform / MessageType members travel as their .value strings; sources
 *   and events travel as plain attribute dicts (the oracle materializes
 *   SimpleNamespace / EventDouble and projects back identically).
 * - Pairing stores / session stores / notifiers travel as id strings; their
 *   methods fire "pairing_store.*" / "session_store.*" / "notifier.*" seams.
 * - Adapters travel as {"id": str, "retire": bool} (non-empty) or null; the
 *   oracle builds a per-case class carrying retire_clarify_card only when
 *   retire is true (callable(getattr(type(adapter), ...)) gate).
 * - Agents travel as the sentinel token "__agent_pending_sentinel__" or
 *   {"id": str, "activity": bool} (non-empty); the oracle builds a per-case
 *   class carrying get_activity_summary only when activity is true.
 * - Authorities travel as {"id", "profile_id", "sessions": {sid: token},
 *   "_ordered_writes": token|null}; rows/handles/pending-clarify/send-results
 *   travel as plain dicts the oracle wraps in SimpleNamespace.
 * - SessionRef / SessionActor construction is SILENT (pure frozen dataclasses);
 *   they travel in seam args as {"profile_id","session_id"} and
 *   {"subject","profile_id","capabilities"(sorted),"transport_id"}.
 * - logger calls are silent on both sides (not traced).
 * - asyncio.to_thread runs its callable synchronously (no trace of its own).
 * - self._session_state is a get-or-create STATE accessor, _peek_session_state
 *   a peek STATE accessor: both record the event (ret = node copy at access
 *   time) and return a borrowed pointer so writes persist into out.state.
 *   State attribute misses raise AttributeError with the BARE key (StateNS).
 * - EphemeralReply is a str subclass: construction is silent, the value
 *   travels as the plain text string (null text -> "None", str(None)).
 * - _hermes_home reads state.hermes_home silently (default ""); the three
 *   Path operations of _hm_write_update_response are path.* seams.
 * - scoped_runtime_main is a sync CM: construction seam + __enter__/__exit__
 *   seams; __exit__ receives {"exc": null|raise-projection} and a TRUTHY
 *   script result suppresses the body raise (function then returns null).
 * - ordered (_ordered_writes) is an async CM: __aenter__/__aexit__ seams.
 * - out.event echoes the (possibly _clarify_superseded-mutated) INPUT event.
 *
 * Fixture domain (documented exclusions):
 * - ASCII text inputs; the U+2014 em dash appears only inside the live
 *   media-file note literals. Integral numbers only at str()/f-string sites
 *   (activity counts, epochs); no integral-float str() divergence.
 * - No NUL characters in fixture strings: libjson strings are NUL-terminated,
 *   so clarify SKIPPED ("\x00skipped") responses are outside the C domain.
 *   The SKIPPED branch is behaviorally identical to a falsy response (both
 *   answer with the raw reply), so no distinguishable behavior is lost.
 * - int() tokens for model.context_length: optional sign + digits, ASCII
 *   whitespace padding, no underscores, |v| <= 2^53, no quote/backslash
 *   chars (ValueError message embeds repr).
 * - POSIX paths without dot segments; home joins append a component.
 * - Store / adapter / agent / authority scripts: null or NON-EMPTY records
 *   (an empty {} double is truthy live but falsy in C).
 * - source carries platform/chat_id/user_id/user_name; event carries text
 *   and message_type; media_urls/media_types present when read unguarded.
 * - session_states[key].turn carries started_ts/agent/ctx when the evict /
 *   running flows read them; .persistent present when native paths buffer.
 * - t() / reply builders / note builders return strings; guess_type scripts
 *   return [type, encoding] pairs; transcripts / paths are string lists.
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

/* ── module constants ── */
/* gateway/run.py:_AGENT_PENDING_SENTINEL marker (identity == token equality) */
#define RI3_SENTINEL "__agent_pending_sentinel__"
/* run_inbound._RELAY_MEDIA_PREFIX / _UNKNOWN_ATTACHMENT_NAME */
#define RI3_RELAY_PREFIX "relay_media_"
#define RI3_UNKNOWN_NAME "(original filename unknown)"
/* tools/clarify_gateway.py outcome constants */
#define RI3_TEXT_RESOLVED "resolved"
#define RI3_TEXT_REJECTED_PROSE "rejected_prose"
#define RI3_TEXT_REJECTED_SELECTION "rejected_selection"
/* SKIPPED == "\x00skipped"; NUL strings are outside the fixture domain, so
 * the equality is transcribed NUL-aware (false for every in-domain value). */
static bool ri3_is_skipped(const char *s) {
    return s && s[0] == '\0' && strcmp(s + 1, "skipped") == 0;
}

/* ── universal seam context ── */
typedef struct ri3_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ri3_ctx_t;

/* ── basic helpers ── */
static bool ri3_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ri3_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool ri3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *ri3_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ri3_str_eq(const json_t *v, const char *s) {
    const char *t = ri3_str(v);
    return t && s && strcmp(t, s) == 0;
}

static char *ri3_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* str.strip(): ASCII whitespace both ends (fixtures are ASCII). */
static char *ri3_strip(const char *s) {
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

static bool ri3_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

/* ── exception model ── */
static json_t *ri3_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/*
 * FULL builtin exception hierarchy (py3.14). ri3_exc_parent walks to the
 * root; `except Exception` catches exactly the kinds whose chain reaches
 * Exception. SystemExit / KeyboardInterrupt / GeneratorExit parent to
 * BaseException ONLY, so they pass through every `except Exception` here.
 * Unknown custom kinds parent to Exception (the oracle's make_exc builds
 * type(kind, (Exception,), {}) for them).
 */
static const char *ri3_exc_parent(const char *t) {
    static const struct { const char *child, *parent; } TAB[] = {
        /* BaseException direct children */
        {"Exception", "BaseException"},
        {"SystemExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        /* Exception direct children */
        {"ArithmeticError", "Exception"},
        {"AssertionError", "Exception"},
        {"AttributeError", "Exception"},
        {"BufferError", "Exception"},
        {"LookupError", "Exception"},
        {"MemoryError", "Exception"},
        {"NameError", "Exception"},
        {"OSError", "Exception"},
        {"ReferenceError", "Exception"},
        {"RuntimeError", "Exception"},
        {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"},
        {"SyntaxError", "Exception"},
        {"SystemError", "Exception"},
        {"TypeError", "Exception"},
        {"ValueError", "Exception"},
        {"Warning", "Exception"},
        {"ImportError", "Exception"},
        /* ExceptionGroup is-a Exception semantically (except Exception catches
         * it); its first direct base is BaseExceptionGroup, but the single-
         * parent walk must reach Exception for the catch contract. */
        {"ExceptionGroup", "Exception"},
        {"EOFError", "Exception"},
        /* ArithmeticError */
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        /* LookupError */
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        /* NameError */
        {"UnboundLocalError", "NameError"},
        /* OSError family (incl. the old EnvironmentError/IOError aliases) */
        {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"},
        {"ConnectionError", "OSError"},
        {"FileExistsError", "OSError"},
        {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"},
        {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"},
        {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"},
        {"TimeoutError", "OSError"},
        /* legacy aliases (IOError/EnvironmentError/WindowsError are OSError) */
        {"IOError", "OSError"},
        {"EnvironmentError", "OSError"},
        {"WindowsError", "OSError"},
        {"BrokenPipeError", "ConnectionError"},
        {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"},
        {"ConnectionResetError", "ConnectionError"},
        /* RuntimeError */
        {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"PythonFinalizationError", "RuntimeError"},
        /* SyntaxError */
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"_IncompleteInputError", "SyntaxError"},
        /* ValueError */
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
        /* ImportError */
        {"ModuleNotFoundError", "ImportError"},
        /* Warning family */
        {"DeprecationWarning", "Warning"},
        {"PendingDeprecationWarning", "Warning"},
        {"RuntimeWarning", "Warning"},
        {"SyntaxWarning", "Warning"},
        {"UserWarning", "Warning"},
        {"FutureWarning", "Warning"},
        {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"},
        {"BytesWarning", "Warning"},
        {"EncodingWarning", "Warning"},
        {"ResourceWarning", "Warning"},
        /* quarry custom families */
        {"PersistenceError", "Exception"},
        {"TransientStoreError", "PersistenceError"},
        {"PermanentStoreError", "PersistenceError"},
        {"StoreUnavailable", "TransientStoreError"},
        {"StoreReplaced", "TransientStoreError"},
        {"CorruptStoreError", "PermanentStoreError"},
        {"AuthorityRefusal", "PersistenceError"},
        {"TurnLeaseTimeoutError", "TimeoutError"},
    };
    for (size_t i = 0; i < sizeof(TAB) / sizeof(TAB[0]); i++)
        if (strcmp(t, TAB[i].child) == 0) return TAB[i].parent;
    return NULL;
}

/* except Exception: the kind's chain reaches Exception (not the trio). */
static bool ri3_is_exception(const json_t *r) {
    if (!ri3_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(t, "Exception") == 0) return true;
        if (strcmp(t, "BaseException") == 0) return false;
        const char *p = ri3_exc_parent(t);
        if (!p) return true;  /* unknown custom kind: make_exc bases Exception */
        t = p;
    }
    return true;
}

static bool ri3_is_kind(const json_t *r, const char *kind) {
    return ri3_is_raise(r) && ri3_str_eq(json_obj_get(r, "__raise__"), kind);
}

/* kind chain reaches `ancestor` (suppress(AuthorityRefusal) etc.). */
static bool ri3_type_is_a(const char *t, const char *ancestor) {
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(t, ancestor) == 0) return true;
        const char *p = ri3_exc_parent(t);
        if (!p) return false;
        t = p;
    }
    return false;
}

static bool ri3_kind_is_a(const json_t *r, const char *ancestor) {
    if (!ri3_is_raise(r)) return false;
    return ri3_type_is_a(json_get_str(r, "__raise__", ""), ancestor);
}

static bool ri3_is_oserror(const json_t *r) { return ri3_kind_is_a(r, "OSError"); }
static bool ri3_is_refusal(const json_t *r) { return ri3_kind_is_a(r, "AuthorityRefusal"); }

/* Python type name of a JSON value (for AttributeError/TypeError messages). */
static const char *ri3_py_type(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* obj.method(...) on a non-carrier: AttributeError naming the method. */
static json_t *ri3_attr_err(const json_t *v, const char *method) {
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'",
             ri3_py_type(v), method);
    return ri3_err("AttributeError", buf);
}

/* StateNS attribute miss: AttributeError with the BARE key. */
static json_t *ri3_state_attr_err(const char *key) {
    return ri3_err("AttributeError", key ? key : "");
}

/* SimpleNamespace attribute miss (pending/rows/handle/authority doubles). */
static json_t *ri3_ns_attr_err(const char *key) {
    char buf[256];
    snprintf(buf, sizeof buf,
             "'types.SimpleNamespace' object has no attribute '%s'", key ? key : "");
    return ri3_err("AttributeError", buf);
}

/* Attribute miss on a record carrier: dict-backed doubles raise the full
 * SimpleNamespace message; a non-record carrier names its own type. */
static json_t *ri3_field_err(const json_t *obj, const char *key) {
    if (obj && obj->type == JSON_OBJECT) return ri3_ns_attr_err(key);
    return ri3_attr_err(obj, key);
}

/*
 * persistence.core.errors._one_line: whitespace collapses to single spaces,
 * other non-printables drop, bounded; empty -> None (ASCII fixture corpus).
 */
static char *ri3_one_line(const char *value, size_t limit) {
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
static json_t *ri3_refusal(const char *reason, const char *field, const char *detail) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "AuthorityRefusal");
    json_set_hdr(e, "message", reason ? reason : "");
    json_set_hdr(e, "reason", reason ? reason : "");
    char *f = ri3_one_line(field, 80);
    json_set(e, "field", f ? json_string(f) : json_null());
    free(f);
    char *d = ri3_one_line(detail, 160);
    json_set(e, "detail", d ? json_string(d) : json_null());
    free(d);
    return e;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *ri3_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    if (strcmp(type, "AuthorityRefusal") == 0) {
        return ri3_refusal(json_get_str(spec, "reason", ""),
                           ri3_str(json_obj_get(spec, "field")),
                           ri3_str(json_obj_get(spec, "detail")));
    }
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && ri3_type_is_a(type, "OSError")) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = ri3_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return ri3_err(type, msg);
}

/* ── seam dispatch ── */
/*
 * One scripted entry: a plain value (reused), or {"__script__": true,
 * "ret": v, "raise": spec}. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. A bare JSON array is a SEQUENCE: one result
 * per call, indexed by a per-name cursor.
 */
static json_t *ri3_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ri3_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *ri3_call(ri3_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = ri3_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = ri3_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = ri3_entry_result(json_get(entry, idx));
        }
    } else {
        res = ri3_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* Propagate a raise result unless it is NULL (success). */
#define RI3_CHK(v) do { if (ri3_is_raise(v)) return (v); } while (0)

/* ── small arg builders ── */
static json_t *ri3_arg1(const char *k1, const json_t *v1) {
    json_t *a = json_object();
    json_set(a, k1, v1 ? json_copy(v1) : json_null());
    return a;
}
static json_t *ri3_arg2(const char *k1, const json_t *v1,
                        const char *k2, const json_t *v2) {
    json_t *a = ri3_arg1(k1, v1);
    json_set(a, k2, v2 ? json_copy(v2) : json_null());
    return a;
}
static json_t *ri3_arg3(const char *k1, const json_t *v1,
                        const char *k2, const json_t *v2,
                        const char *k3, const json_t *v3) {
    json_t *a = ri3_arg2(k1, v1, k2, v2);
    json_set(a, k3, v3 ? json_copy(v3) : json_null());
    return a;
}
static json_t *ri3_args_str(const char *k, const char *v) {
    json_t *a = json_object();
    json_set_hdr(a, k, v ? v : "");
    return a;
}

/* tuple/list builders */
static json_t *ri3_pair(const json_t *a, const json_t *b) {
    json_t *t = json_array();
    json_append(t, a ? json_copy(a) : json_null());
    json_append(t, b ? json_copy(b) : json_null());
    return t;
}

/* a, b = result — ValueError/TypeError messages per py3.14. */
static bool ri3_unpack2(const json_t *r, json_t **out) {
    if (r && r->type == JSON_ARRAY && json_len(r) == 2) {
        out[0] = json_get(r, 0);
        out[1] = json_get(r, 1);
        return true;
    }
    size_t got = (r && r->type == JSON_ARRAY) ? json_len(r) : 0;
    char buf[128];
    if (!r || r->type != JSON_ARRAY) {
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object",
                 ri3_py_type(r));
        out[0] = ri3_err("TypeError", buf);
        return false;
    }
    if (got < 2) {
        snprintf(buf, sizeof buf,
                 "not enough values to unpack (expected 2, got %zu)", got);
    } else {
        snprintf(buf, sizeof buf,
                 "too many values to unpack (expected 2, got %zu)", got);
    }
    out[0] = ri3_err("ValueError", buf);
    return false;
}

/* ── state helpers ── */
static json_t *ri3_sub(ri3_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

/* self._session_state: get-or-create STATE accessor (traced, borrowed). */
static json_t *ri3_session_state(ri3_ctx_t *c, const char *key) {
    json_t *states = ri3_sub(c, "session_states");
    if (!states) {
        states = json_object();
        json_set(c->state, "session_states", states);
    }
    json_t *node = key ? json_obj_get(states, key) : NULL;
    if (!node || node->type != JSON_OBJECT) {
        node = json_object();
        json_set(states, key ? key : "", node);
    }
    json_t *a = ri3_args_str("key", key);
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._session_state");
    json_set(ev, "args", a);
    json_set(ev, "ret", json_copy(node));
    json_append(c->trace, ev);
    return node;
}

/* self._peek_session_state: peek STATE accessor (traced, borrowed, no create). */
static json_t *ri3_peek_session_state(ri3_ctx_t *c, const char *key) {
    json_t *states = ri3_sub(c, "session_states");
    json_t *node = (states && key) ? json_obj_get(states, key) : NULL;
    if (node && node->type != JSON_OBJECT) node = NULL;
    json_t *a = ri3_args_str("key", key);
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._peek_session_state");
    json_set(ev, "args", a);
    json_set(ev, "ret", node ? json_copy(node) : json_null());
    json_append(c->trace, ev);
    return node;
}

/* event.get_command(): EventDouble seam. */
static json_t *ri3_ev_command(ri3_ctx_t *c) {
    return ri3_call(c, "event.get_command", json_object());
}

/* t(key, **kw) seam. */
static json_t *ri3_t(ri3_ctx_t *c, const char *key) {
    return ri3_call(c, "t", ri3_args_str("key", key));
}

/* ── POSIX path / string helpers (ASCII fixture domain) ── */
static const char *ri3_basename(const char *p) {
    if (!p) return "";
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* os.path.splitext(path)[1] — leading-dot basenames carry no extension. */
static char *ri3_splitext(const char *path) {
    if (!path) return ri3_strdup("");
    const char *base = ri3_basename(path);
    const char *dot = strrchr(path, '.');
    if (dot && dot > base) {
        const char *q = base;
        while (q < dot && *q == '.') q++;
        if (q < dot) return ri3_strdup(dot);
    }
    return ri3_strdup("");
}

/* PurePosixPath(home) / name — component append (no dot segments in domain). */
static char *ri3_home_join(const char *home, const char *name) {
    if (!home || !*home) return ri3_strdup(name);
    size_t n = strlen(home);
    bool sep = home[n - 1] == '/';
    size_t m = strlen(name);
    char *o = malloc(n + (sep ? 0 : 1) + m + 1);
    if (!o) return NULL;
    memcpy(o, home, n);
    size_t k = n;
    if (!sep) o[k++] = '/';
    memcpy(o + k, name, m + 1);
    return o;
}

/* re.sub(r'[^\w.\- ]', '_', name) — ASCII \w == [A-Za-z0-9_]. */
static char *ri3_sanitize_name(const char *name) {
    if (!name) name = "";
    size_t n = strlen(name);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)name[i];
        bool keep = isalnum(ch) || ch == '_' || ch == '.' || ch == '-' || ch == ' ';
        o[i] = keep ? (char)ch : '_';
    }
    o[n] = '\0';
    return o;
}

/* basename.split("_", 2)[2] when >= 3 parts, else basename. */
static char *ri3_cache_name(const char *basename) {
    const char *p1 = strchr(basename, '_');
    if (p1) {
        const char *p2 = strchr(p1 + 1, '_');
        if (p2) return ri3_strdup(p2 + 1);
    }
    return ri3_strdup(basename);
}

/* int(v) for the context_length pin: bool/number/string domain.
 * Returns a number node, or NULL with *err set (ValueError/TypeError). */
static json_t *ri3_int_coerce(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = ri3_err("TypeError",
                       "int() argument must be a string, a bytes-like object "
                       "or a real number, not 'NoneType'");
        return NULL;
    }
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) {
            *err = ri3_err(d != d ? "ValueError" : "OverflowError",
                           d != d ? "cannot convert float NaN to integer"
                                  : "cannot convert float infinity to integer");
            return NULL;
        }
        return json_number((double)(long long)d);
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) end--;
        bool neg = false;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        if (p == end) {
            char buf[512];
            snprintf(buf, sizeof buf,
                     "invalid literal for int() with base 10: '%s'", s);
            *err = ri3_err("ValueError", buf);
            return NULL;
        }
        long long acc = 0;
        for (const char *q = p; q < end; q++) {
            if (!isdigit((unsigned char)*q)) {
                char buf[512];
                snprintf(buf, sizeof buf,
                         "invalid literal for int() with base 10: '%s'", s);
                *err = ri3_err("ValueError", buf);
                return NULL;
            }
            acc = acc * 10 + (*q - '0');
            if (acc > 9007199254740992LL) {
                char buf[512];
                snprintf(buf, sizeof buf,
                         "invalid literal for int() with base 10: '%s'", s);
                *err = ri3_err("ValueError", buf);
                return NULL;
            }
        }
        return json_number((double)(neg ? -acc : acc));
    }
    char buf[256];
    snprintf(buf, sizeof buf,
             "int() argument must be a string, a bytes-like object "
             "or a real number, not '%s'", ri3_py_type(v));
    *err = ri3_err("TypeError", buf);
    return NULL;
}

/* JSON scalar equality for the model != configured_model comparison. */
static bool ri3_val_eq(const json_t *a, const json_t *b) {
    bool an = ri3_is_null(a), bn = ri3_is_null(b);
    if (an || bn) return an && bn;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        default: return false;   /* containers: outside the model-name domain */
    }
}

/* f"{x:.0f}" — printf %.0f is half-to-even under the default rounding mode,
 * matching Python format(); inf prints "inf" on both sides. */
static void ri3_fmt0f(char *buf, size_t cap, double d) {
    snprintf(buf, cap, "%.0f", d);
}

/* str() of an integral-domain number for f-string interpolation. */
static void ri3_num_str(char *buf, size_t cap, const json_t *v) {
    if (v && v->type == JSON_NUMBER) snprintf(buf, cap, "%.0f", v->num_val);
    else if (v && v->type == JSON_STRING) snprintf(buf, cap, "%s", v->str_val ? v->str_val : "");
    else if (v && v->type == JSON_BOOL) snprintf(buf, cap, "%s", v->bool_val ? "True" : "False");
    else if (ri3_is_null(v)) snprintf(buf, cap, "None");
    else snprintf(buf, cap, "%s", "");
}

/* ══════════════ ported methods ══════════════ */

/* forward decls for the composed siblings */
static json_t *ri3_prepare_clarify_reply_text(ri3_ctx_t *c, json_t *args);
static json_t *ri3_inbound_attachment_display_name(ri3_ctx_t *c, json_t *args);

/* PoP: ri3_hm_offer_pairing_code @ gateway/run_inbound.py:_hm_offer_pairing_code */
static json_t *ri3_hm_offer_pairing_code(ri3_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    if (!src || src->type != JSON_OBJECT) {
        return ri3_attr_err(src, "platform");
    }
    json_t *plat = json_obj_get(src, "platform");
    const char *platform = ri3_truthy(plat) ? ri3_str(plat) : NULL;
    if (!platform) platform = "unknown";
    /* platform.value read on a non-str truthy platform is outside the domain */

    json_t *store = ri3_call(c, "self._pairing_store_for", ri3_arg1("source", src));
    RI3_CHK(store);
    if (ri3_is_null(store)) {
        json_free(store);
        return json_null();   /* logger.error silent */
    }
    json_t *uid = json_obj_get(src, "user_id");
    json_t *rl = ri3_call(c, "pairing_store._is_rate_limited",
                          ri3_arg2("platform", json_string(platform), "user_id", uid));
    if (ri3_is_raise(rl)) { json_free(store); return rl; }
    bool limited = ri3_truthy(rl);
    json_free(rl);
    if (limited) { json_free(store); return json_null(); }

    json_t *uname = json_obj_get(src, "user_name");
    const char *un = ri3_truthy(uname) ? ri3_str(uname) : NULL;
    json_t *code = ri3_call(c, "pairing_store.generate_code",
                            ri3_arg3("platform", json_string(platform),
                                     "user_id", uid,
                                     "user_name", json_string(un ? un : "")));
    if (ri3_is_raise(code)) {
        if (ri3_is_oserror(code)) {   /* except (OSError, TimeoutError) */
            json_free(code);
            json_free(store);
            return json_null();       /* logger.warning silent */
        }
        json_free(store);
        return code;                  /* BaseException / other passthrough */
    }

    json_t *adapter = ri3_call(c, "self._delivery_adapter_for", ri3_arg1("source", src));
    if (ri3_is_raise(adapter)) { json_free(code); json_free(store); return adapter; }

    json_t *reply;
    if (ri3_truthy(code)) {
        json_t *prof = ri3_call(c, "pairing_profile_arg", ri3_arg1("store", store));
        if (ri3_is_raise(prof)) { json_free(adapter); json_free(code); json_free(store); return prof; }
        reply = ri3_call(c, "pairing_code_reply",
                         ri3_arg3("platform", json_string(platform),
                                  "code", code, "profile", prof));
        json_free(prof);
    } else {
        reply = ri3_call(c, "pairing_rate_limited_reply", json_object());
    }
    if (ri3_is_raise(reply)) { json_free(adapter); json_free(code); json_free(store); return reply; }

    if (ri3_truthy(adapter)) {
        json_t *chat = json_obj_get(src, "chat_id");
        json_t *sargs = ri3_arg2("chat_id", chat, "text", reply);
        json_set(sargs, "metadata", json_null());
        json_t *sent = ri3_call(c, "adapter.send", sargs);
        if (ri3_is_raise(sent)) {
            json_free(reply); json_free(adapter); json_free(code); json_free(store);
            return sent;
        }
        json_free(sent);
    }
    if (!ri3_truthy(code)) {
        json_t *rec = ri3_call(c, "pairing_store._record_rate_limit",
                               ri3_arg2("platform", json_string(platform), "user_id", uid));
        if (ri3_is_raise(rec)) {
            json_free(reply); json_free(adapter); json_free(code); json_free(store);
            return rec;
        }
        json_free(rec);
    }
    json_free(reply);
    json_free(adapter);
    json_free(code);
    json_free(store);
    return json_null();
}

/* PoP: ri3_hm_report_ignored_dm @ gateway/run_inbound.py:_hm_report_ignored_dm */
static json_t *ri3_hm_report_ignored_dm(ri3_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    if (!src || src->type != JSON_OBJECT) {
        return ri3_attr_err(src, "platform");
    }
    json_t *plat = json_obj_get(src, "platform");
    const char *platform = ri3_truthy(plat) ? ri3_str(plat) : NULL;
    if (!platform) platform = "unknown";
    json_t *uid = json_obj_get(src, "user_id");
    json_t *uname = json_obj_get(src, "user_name");
    const char *un = ri3_truthy(uname) ? ri3_str(uname) : NULL;

    /* display_hermes_home() evaluates as an argument BEFORE the hint call */
    json_t *home = ri3_call(c, "display_hermes_home", json_object());
    RI3_CHK(home);
    json_t *hargs = json_object();
    json_set_hdr(hargs, "platform", platform);
    json_set(hargs, "user_id", uid ? json_copy(uid) : json_null());
    json_set_hdr(hargs, "user_name", un ? un : "");
    json_set(hargs, "hermes_home", home);
    json_t *hint = ri3_call(c, "unauthorized_owner_hint", hargs);
    RI3_CHK(hint);
    /* logger.warning silent */

    json_t *notifier = json_obj_get(c->state, "_unauthorized_owner_notifier");
    if (ri3_is_null(notifier)) {
        json_t *made = ri3_call(c, "UnauthorizedOwnerNotifier", json_object());
        RI3_CHK(made);
        json_set(c->state, "_unauthorized_owner_notifier", json_copy(made));
        notifier = json_obj_get(c->state, "_unauthorized_owner_notifier");
    }
    if (ri3_is_null(notifier)) {
        /* scripted null constructor: None.first_time raises */
        json_free(hint);
        return ri3_attr_err(NULL, "first_time");
    }
    json_t *ft = ri3_call(c, "notifier.first_time",
                          ri3_arg2("platform", json_string(platform), "user_id", uid));
    if (ri3_is_raise(ft)) { json_free(hint); return ft; }
    bool first = ri3_truthy(ft);
    json_free(ft);
    json_t *cfg = json_obj_get(c->state, "config");
    if (first && !ri3_is_null(cfg)) {
        json_t *nargs = ri3_arg2("source", src, "hint", hint);
        json_t *sent = ri3_call(c, "notifier.notify", nargs);
        if (ri3_is_raise(sent)) { json_free(hint); return sent; }
        json_free(sent);
    }
    json_free(hint);
    return json_null();
}

/* PoP: ri3_hm_write_update_response @ gateway/run_inbound.py:_hm_write_update_response */
static json_t *ri3_hm_write_update_response(ri3_ctx_t *c, json_t *args) {
    const char *home = json_get_str(c->state, "hermes_home", "");
    json_t *text = json_obj_get(args, "response_text");
    char *resp = ri3_home_join(home, ".update_response");
    /* Path(".update_response").with_suffix(".tmp"): a leading-dot name has
     * no suffix, so the suffix is APPENDED. */
    size_t rn = strlen(resp);
    char *tmp = malloc(rn + 5);
    if (tmp) { memcpy(tmp, resp, rn); memcpy(tmp + rn, ".tmp", 5); }
    char *prompt = ri3_home_join(home, ".update_prompt.json");

    json_t *r = ri3_call(c, "path.write_text",
                         ri3_arg2("path", json_string(tmp ? tmp : ""), "text", text));
    if (ri3_is_raise(r)) {
        if (ri3_is_oserror(r)) {
            json_t *out = json_string(json_get_str(r, "message", ""));
            json_free(r); free(resp); free(tmp); free(prompt);
            return out;
        }
        free(resp); free(tmp); free(prompt);
        return r;
    }
    json_free(r);
    r = ri3_call(c, "path.replace",
                 ri3_arg2("src", json_string(tmp ? tmp : ""),
                          "dst", json_string(resp)));
    if (ri3_is_raise(r)) {
        if (ri3_is_oserror(r)) {
            json_t *out = json_string(json_get_str(r, "message", ""));
            json_free(r); free(resp); free(tmp); free(prompt);
            return out;
        }
        free(resp); free(tmp); free(prompt);
        return r;
    }
    json_free(r);
    json_t *uargs = ri3_arg1("path", json_string(prompt ? prompt : ""));
    json_set(uargs, "missing_ok", json_bool(true));
    r = ri3_call(c, "path.unlink", uargs);
    if (ri3_is_raise(r)) {
        if (ri3_is_oserror(r)) {
            json_t *out = json_string(json_get_str(r, "message", ""));
            json_free(r); free(resp); free(tmp); free(prompt);
            return out;
        }
        free(resp); free(tmp); free(prompt);
        return r;
    }
    json_free(r);
    free(resp); free(tmp); free(prompt);
    return json_null();
}

/* PoP: ri3_prepare_clarify_reply_text @ gateway/run_inbound.py:_prepare_clarify_reply_text */
static json_t *ri3_prepare_clarify_reply_text(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *typed = (ev && ev->type == JSON_OBJECT)
        ? json_obj_get(ev, "_telegram_replied_voice_typed_text") : NULL;
    if (ri3_truthy(typed)) return json_copy(typed);

    json_t *paths = ri3_call(c, "self._pending_event_audio_paths", ri3_arg1("event", ev));
    RI3_CHK(paths);
    bool has_audio = ri3_truthy(paths);
    json_free(paths);
    if (!has_audio) {
        json_t *txt = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "text") : NULL;
        if (!txt) {
            if (!ev || ev->type != JSON_OBJECT) return ri3_attr_err(ev, "text");
            return ri3_state_attr_err("text");
        }
        const char *s = ri3_truthy(txt) ? ri3_str(txt) : NULL;
        char *st = ri3_strip(s ? s : "");
        json_t *out = json_string(st ? st : "");
        free(st);
        return out;
    }
    json_t *targs = ri3_arg1("event", ev);
    json_set_hdr(targs, "arg", "");
    json_t *pair = ri3_call(c, "self._transcribe_pending_audio_event_once", targs);
    RI3_CHK(pair);
    json_t *parts[2];
    if (!ri3_unpack2(pair, parts)) {
        json_t *e = parts[0];
        json_free(pair);
        return e;
    }
    json_t *transcripts = parts[1];
    /* "\n\n".join(t.strip() for t in transcripts if t.strip()) */
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) { json_free(pair); return json_null(); }
    buf[0] = '\0';
    bool first = true;
    if (transcripts && transcripts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(transcripts); i++) {
            json_t *tx = json_get(transcripts, i);
            const char *s = ri3_str(tx);
            if (!s) {
                free(buf);
                json_t *e = ri3_attr_err(tx, "strip");
                json_free(pair);
                return e;
            }
            char *st = ri3_strip(s);
            if (st && st[0]) {
                size_t sl = strlen(st);
                size_t need = len + sl + (first ? 0 : 2) + 1;
                if (need > cap) {
                    while (cap < need) cap *= 2;
                    char *nb = realloc(buf, cap);
                    if (!nb) { free(st); free(buf); json_free(pair); return json_null(); }
                    buf = nb;
                }
                if (!first) { memcpy(buf + len, "\n\n", 2); len += 2; }
                memcpy(buf + len, st, sl);
                len += sl;
                buf[len] = '\0';
                first = false;
            }
            free(st);
        }
    } else if (!ri3_is_null(transcripts)) {
        free(buf);
        char m[128];
        snprintf(m, sizeof m, "'%s' object is not iterable", ri3_py_type(transcripts));
        json_t *e = ri3_err("TypeError", m);
        json_free(pair);
        return e;
    }
    json_t *out = json_string(buf);
    free(buf);
    json_free(pair);
    return out;
}

/* PoP: ri3_hm_clarify_reply @ gateway/run_inbound.py:_hm_clarify_reply */
static json_t *ri3_hm_clarify_reply(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");

    /* try: get_pending_for_session ... except Exception: return None */
    json_t *pargs = ri3_args_str("key", key);
    json_set(pargs, "include_choice_prompts", json_bool(true));
    json_t *pending = ri3_call(c, "clarify.get_pending_for_session", pargs);
    if (ri3_is_raise(pending)) {
        if (ri3_is_exception(pending)) { json_free(pending); return json_null(); }
        return pending;   /* BaseException passthrough */
    }
    if (ri3_is_null(pending)) { json_free(pending); return json_null(); }
    if (pending->type != JSON_OBJECT) {
        json_t *e = ri3_attr_err(pending, "clarify_id");
        json_free(pending);
        return e;
    }
    json_t *cid = json_obj_get(pending, "clarify_id");
    if (!cid) {
        json_free(pending);
        return ri3_ns_attr_err("clarify_id");
    }
    json_t *refused = ri3_call(c, "self._refused_clarify",
                               ri3_arg2("event", ev, "clarify_id", cid));
    if (ri3_is_raise(refused)) { json_free(pending); return refused; }
    bool ref = ri3_truthy(refused);
    json_free(refused);
    if (ref) { json_free(pending); return json_null(); }

    json_t *audio = ri3_call(c, "self._pending_event_audio_paths", ri3_arg1("event", ev));
    if (ri3_is_raise(audio)) { json_free(pending); return audio; }
    bool has_audio = ri3_truthy(audio);
    json_free(audio);

    json_t *rargs = json_object();
    json_set(rargs, "event", ev ? json_copy(ev) : json_null());
    json_t *raw = ri3_prepare_clarify_reply_text(c, rargs);
    json_free(rargs);
    if (ri3_is_raise(raw)) { json_free(pending); return raw; }
    const char *raw_s = ri3_str(raw);

    if (has_audio && !ri3_truthy(raw)) {
        json_free(raw); json_free(pending);
        return json_string("");   /* _retain: logger.info silent */
    }
    if (!ri3_truthy(raw) || (raw_s && raw_s[0] == '/')) {
        json_free(raw); json_free(pending);
        return json_null();
    }

    json_t *outcome = ri3_call(c, "clarify.attempt_text_response",
                               ri3_arg2("clarify_id", cid, "text", raw));
    if (ri3_is_raise(outcome)) { json_free(raw); json_free(pending); return outcome; }

    if (ri3_str_eq(outcome, RI3_TEXT_RESOLVED)) {
        json_free(outcome);
        json_t *adapter = ri3_call(c, "self._delivery_adapter_for", ri3_arg1("source", src));
        if (ri3_is_raise(adapter)) { json_free(raw); json_free(pending); return adapter; }
        if (ri3_truthy(adapter)) {
            json_t *chat = (src && src->type == JSON_OBJECT) ? json_obj_get(src, "chat_id") : NULL;
            json_t *rt = ri3_call(c, "adapter.resume_typing_for_chat", ri3_arg1("chat_id", chat));
            if (ri3_is_raise(rt)) {
                if (!ri3_is_exception(rt)) {
                    json_free(adapter); json_free(raw); json_free(pending);
                    return rt;   /* BaseException passthrough */
                }
                json_free(rt);   /* except Exception: logger.debug silent */
            } else {
                json_free(rt);
            }
            /* callable(getattr(type(adapter), "retire_clarify_card", None)) */
            bool has_retire = adapter->type == JSON_OBJECT &&
                ri3_truthy(json_obj_get(adapter, "retire"));
            if (has_retire) {
                /* response read + t() + retire all ride INSIDE the try:
                 * an Exception-family raise (incl AttributeError on a missing
                 * response) is swallowed; BaseException passes through. */
                json_t *resp = json_obj_get(pending, "response");
                if (!resp) {
                    /* AttributeError('response') swallowed by except Exception */
                    goto clarify_resolved_done;
                }
                const char *resp_s = ri3_str(resp);
                const char *answer;
                if (ri3_is_skipped(resp_s)) answer = raw_s ? raw_s : "";
                else answer = ri3_truthy(resp) ? (resp_s ? resp_s : "") : (raw_s ? raw_s : "");
                json_t *targs = ri3_args_str("key", "gateway.clarify.answered");
                json_set_hdr(targs, "response", answer);
                json_t *txt = ri3_call(c, "t", targs);
                if (ri3_is_raise(txt)) {
                    if (!ri3_is_exception(txt)) {
                        json_free(adapter); json_free(raw); json_free(pending);
                        return txt;
                    }
                    json_free(txt);   /* inside except Exception swallow */
                } else {
                    json_t *retire = ri3_call(c, "adapter.retire_clarify_card",
                                              ri3_arg2("clarify_id", cid, "text", txt));
                    json_free(txt);
                    if (ri3_is_raise(retire)) {
                        if (!ri3_is_exception(retire)) {
                            json_free(adapter); json_free(raw); json_free(pending);
                            return retire;
                        }
                        json_free(retire);
                    } else {
                        json_free(retire);
                    }
                }
            }
        }
clarify_resolved_done:
        json_free(adapter);
        json_free(raw);
        json_free(pending);
        return json_string("");
    }
    if (ri3_str_eq(outcome, RI3_TEXT_REJECTED_SELECTION)) {
        json_free(outcome); json_free(raw); json_free(pending);
        return json_string("");   /* _retain */
    }
    if (ri3_str_eq(outcome, RI3_TEXT_REJECTED_PROSE)) {
        json_free(outcome);
        json_t *gen = ri3_call(c, "self._current_session_run_generation",
                               ri3_args_str("key", key));
        if (ri3_is_raise(gen)) { json_free(raw); json_free(pending); return gen; }
        json_t *rel = ri3_call(c, "self._release_pending_clarify",
                               ri3_arg2("pending", pending, "source", src));
        if (ri3_is_raise(rel)) { json_free(gen); json_free(raw); json_free(pending); return rel; }
        bool released = ri3_truthy(rel);
        json_free(rel);
        if (released && ev && ev->type == JSON_OBJECT) {
            json_set(ev, "_clarify_superseded", gen ? json_copy(gen) : json_null());
        }
        json_free(gen);
        json_free(raw);
        json_free(pending);
        return json_null();
    }
    json_free(outcome);
    json_free(raw);
    json_free(pending);
    return json_null();
}

/* PoP: ri3_hm_evict_idle_stale_agent @ gateway/run_inbound.py:_hm_evict_idle_stale_agent */
static json_t *ri3_hm_evict_idle_stale_agent(ri3_ctx_t *c, json_t *args) {
    const char *key = json_get_str(args, "key", "");

    json_t *fargs = ri3_args_str("name", "HERMES_AGENT_TIMEOUT");
    json_set(fargs, "default", json_number(1800));
    json_t *raw_to = ri3_call(c, "_float_env", fargs);
    RI3_CHK(raw_to);
    double timeout = (raw_to && raw_to->type == JSON_NUMBER) ? raw_to->num_val : 0.0;
    json_free(raw_to);

    json_t *qs = ri3_peek_session_state(c, key);
    double stale_ts = 0.0;
    if (qs) {
        json_t *turn0 = json_obj_get(qs, "turn");
        if (!turn0 || turn0->type != JSON_OBJECT) return ri3_state_attr_err("turn");
        json_t *ts = json_obj_get(turn0, "started_ts");
        if (!ts) return ri3_state_attr_err("started_ts");
        stale_ts = (ts->type == JSON_NUMBER) ? ts->num_val : 0.0;
    }
    if (!qs) return json_null();
    json_t *turn = json_obj_get(qs, "turn");
    json_t *agent = json_obj_get(turn, "agent");
    if (!agent) return ri3_state_attr_err("agent");
    if (ri3_is_null(agent) || !ri3_truthy(json_obj_get(turn, "started_ts"))) {
        return json_null();
    }

    json_t *now = ri3_call(c, "time.time", json_object());
    RI3_CHK(now);
    double now_v = (now && now->type == JSON_NUMBER) ? now->num_val : 0.0;
    json_free(now);
    double stale_age = now_v - stale_ts;

    bool is_sentinel = ri3_str_eq(agent, RI3_SENTINEL);
    double stale_idle = INFINITY;
    bool valid = false;
    char detail[1024];
    detail[0] = '\0';

    /* if agent and hasattr(agent, "get_activity_summary"): with suppress(Exception) */
    bool has_activity = !is_sentinel && ri3_truthy(agent) &&
        agent->type == JSON_OBJECT && ri3_truthy(json_obj_get(agent, "activity"));
    if (has_activity) {
        /* saved summary fields for the detail f-string (integral domain) */
        char desc[256] = "unknown";
        char cnt[64] = "0", mx[64] = "0";
        json_t *sa = ri3_call(c, "agent.get_activity_summary", json_object());
        if (ri3_is_raise(sa)) {
            if (!ri3_is_exception(sa)) return sa;
            json_free(sa);
            goto after_activity;
        }
        if (sa && sa->type == JSON_OBJECT) {
            json_t *d = json_obj_get(sa, "last_activity_desc");
            if (d) ri3_num_str(desc, sizeof desc, d);
            json_t *cn = json_obj_get(sa, "api_call_count");
            if (cn) ri3_num_str(cnt, sizeof cnt, cn);
            json_t *mxn = json_obj_get(sa, "max_iterations");
            if (mxn) ri3_num_str(mx, sizeof mx, mxn);
        }
        /* resolve_session_idle_seconds_from_activity(sa if dict else None, now=time.time()) */
        json_t *now2 = ri3_call(c, "time.time", json_object());
        if (ri3_is_raise(now2)) {
            if (!ri3_is_exception(now2)) { json_free(sa); return now2; }
            json_free(now2);
            json_free(sa);
            goto after_activity;
        }
        json_t *rargs = json_object();
        json_set(rargs, "activity",
                 (sa && sa->type == JSON_OBJECT) ? json_copy(sa) : json_null());
        json_set(rargs, "now", now2);
        json_t *resolved = ri3_call(c, "resolve_session_idle_seconds_from_activity", rargs);
        json_free(sa);
        if (ri3_is_raise(resolved)) {
            if (!ri3_is_exception(resolved)) return resolved;
            json_free(resolved);
            goto after_activity;
        }
        if (!ri3_is_null(resolved)) {
            stale_idle = (resolved->type == JSON_NUMBER) ? resolved->num_val : 0.0;
            valid = true;
            /* getattr(turn.ctx, "awake_idle", None) — a ctx miss raises
             * AttributeError INSIDE the suppress block (detail stays ""). */
            json_t *ctx = json_obj_get(turn, "ctx");
            if (!ctx || ctx->type != JSON_OBJECT) {
                json_free(resolved);
                goto after_activity;
            }
            json_t *awake = json_obj_get(ctx, "awake_idle");
            if (!ri3_is_null(awake)) {
                json_t *m = ri3_call(c, "awake_idle.measure",
                                     ri3_arg1("idle", json_number(stale_idle)));
                if (ri3_is_raise(m)) {
                    if (!ri3_is_exception(m)) { json_free(resolved); return m; }
                    json_free(m);
                    json_free(resolved);
                    goto after_activity;
                }
                if (m && m->type == JSON_NUMBER) stale_idle = m->num_val;
                json_free(m);
            }
        }
        json_free(resolved);
        /* detail is built whenever the suppress block ran to completion */
        char idle_s[64];
        ri3_fmt0f(idle_s, sizeof idle_s, stale_idle);
        snprintf(detail, sizeof detail,
                 " | last_activity=%s (%ss ago) | iteration=%s/%s",
                 desc, idle_s, cnt, mx);
    }
after_activity:
    ;
    double wall_ttl = (timeout > 0) ? fmax(timeout * 10.0, 7200.0) : INFINITY;
    bool should_evict = !is_sentinel && (
        (valid && timeout > 0 && stale_idle >= timeout) ||
        (!valid && stale_age > wall_ttl));
    if (should_evict) {
        /* logger.warning silent (age/idle/timeout/detail ride the log only) */
        json_t *r = ri3_call(c, "self._hm_evict_running_agent",
                             ri3_arg2("key", json_string(key),
                                      "reason", json_string("stale_running_agent_eviction")));
        RI3_CHK(r);
        json_free(r);
    }
    return json_null();
}

/* PoP: ri3_hm_stop_owner_turn @ gateway/run_inbound.py:_hm_stop_owner_turn */
static json_t *ri3_hm_stop_owner_turn(ri3_ctx_t *c, json_t *args) {
    const char *key = json_get_str(args, "key", "");

    json_t *authority = ri3_call(c, "active_authority", json_object());
    RI3_CHK(authority);
    json_t *sid = NULL;
    if (!ri3_is_null(authority)) {
        if (authority->type != JSON_OBJECT) {
            json_t *e = ri3_attr_err(authority, "logical_owner");
            json_free(authority);
            return e;
        }
        json_t *store = json_obj_get(c->state, "session_store");
        if (ri3_is_null(store)) {
            json_free(authority);
            return ri3_attr_err(NULL, "peek_session_id");
        }
        json_t *peek = ri3_call(c, "session_store.peek_session_id", ri3_args_str("key", key));
        if (ri3_is_raise(peek)) { json_free(authority); return peek; }
        sid = ri3_call(c, "authority.logical_owner", ri3_arg1("sid", peek));
        json_free(peek);
        if (ri3_is_raise(sid)) { json_free(authority); return sid; }
    }
    bool in_sessions = false;
    if (!ri3_is_null(authority) && ri3_truthy(sid)) {
        json_t *sessions = json_obj_get(authority, "sessions");
        if (!sessions) {
            json_free(sid); json_free(authority);
            return ri3_ns_attr_err("sessions");
        }
        const char *sid_s = ri3_str(sid);
        if (sessions->type == JSON_OBJECT && sid_s)
            in_sessions = json_obj_get(sessions, sid_s) != NULL;
    }
    if (!ri3_truthy(sid) || !in_sessions) {
        if (!ri3_is_null(authority)) {
            json_t *f = ri3_call(c, "authority.fence_route", ri3_args_str("key", key));
            if (ri3_is_raise(f)) { json_free(sid); json_free(authority); return f; }
            json_free(f);
        }
        json_free(sid);
        json_free(authority);
        return json_null();
    }

    /* SessionRef / SessionActor: silent frozen-dataclass construction */
    json_t *pid = json_obj_get(authority, "profile_id");
    if (!pid) {
        json_free(sid); json_free(authority);
        return ri3_ns_attr_err("profile_id");
    }
    json_t *ref = json_object();
    json_set(ref, "profile_id", json_copy(pid));
    json_set(ref, "session_id", json_copy(sid));
    json_t *actor = json_object();
    json_set_hdr(actor, "subject", "messaging-control");
    json_set(actor, "profile_id", json_copy(pid));
    json_t *caps = json_array();
    json_append(caps, json_string("session:control"));
    json_append(caps, json_string("session:submit"));
    json_set(actor, "capabilities", caps);
    json_set_hdr(actor, "transport_id", "busy-command");

    json_t *store = ri3_call(c, "authority_persistence", ri3_arg1("authority", authority));
    if (ri3_is_raise(store)) {
        json_free(ref); json_free(actor); json_free(sid); json_free(authority);
        return store;
    }
    if (ri3_is_null(store)) {
        char msg[256];
        snprintf(msg, sizeof msg, "no storage handle for %s",
                 ri3_str(pid) ? ri3_str(pid) : "");
        json_t *e = ri3_err("StoreUnavailable", msg);
        json_free(ref); json_free(actor); json_free(sid); json_free(authority);
        return e;
    }

    json_t *rows = ri3_call(c, "store.admissions.list", ri3_arg1("session_id", sid));
    if (ri3_is_raise(rows)) {
        json_free(store); json_free(ref); json_free(actor); json_free(sid); json_free(authority);
        return rows;
    }
    /* paused = any(row.status == 'unknown' for row in rows) */
    bool paused = false;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *row = json_get(rows, i);
            json_t *st = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "status") : NULL;
            if (!st) {
                json_t *e = ri3_field_err(row, "status");
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return e;
            }
            if (ri3_str_eq(st, "unknown")) { paused = true; break; }
        }
    } else if (!ri3_is_null(rows)) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object is not iterable", ri3_py_type(rows));
        json_t *e = ri3_err("TypeError", m);
        json_free(rows); json_free(store); json_free(ref); json_free(actor);
        json_free(sid); json_free(authority);
        return e;
    }
    if (!paused) {
        json_t *f = ri3_call(c, "authority.fence_route", ri3_args_str("key", key));
        if (ri3_is_raise(f)) {
            json_free(rows); json_free(store); json_free(ref); json_free(actor);
            json_free(sid); json_free(authority);
            return f;
        }
        json_free(f);
        json_t *ordered = json_obj_get(authority, "_ordered_writes");
        if (!ri3_is_null(ordered)) {
            json_t *en = ri3_call(c, "ordered.__aenter__", json_object());
            if (ri3_is_raise(en)) {
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return en;
            }
            json_free(en);
            json_t *ex = ri3_call(c, "ordered.__aexit__", json_object());
            if (ri3_is_raise(ex)) {
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return ex;
            }
            json_free(ex);
        }
        json_free(rows);
        rows = ri3_call(c, "store.admissions.list", ri3_arg1("session_id", sid));
        if (ri3_is_raise(rows)) {
            json_free(store); json_free(ref); json_free(actor); json_free(sid); json_free(authority);
            return rows;
        }
    }
    if (!paused && rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *row = json_get(rows, i);
            json_t *st = (row && row->type == JSON_OBJECT) ? json_obj_get(row, "status") : NULL;
            if (!st) {
                json_t *e = ri3_field_err(row, "status");
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return e;
            }
            if (!ri3_str_eq(st, "queued")) continue;
            json_t *prin = json_obj_get(row, "principal_id");
            if (!prin) {
                json_t *e = ri3_ns_attr_err("principal_id");
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return e;
            }
            const char *prin_s = ri3_str(prin);
            if (!prin_s) {
                json_t *e = ri3_attr_err(prin, "startswith");
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return e;
            }
            if (!ri3_startswith(prin_s, "messaging:")) continue;
            json_t *aid = json_obj_get(row, "admission_id");
            if (!aid) {
                json_t *e = ri3_ns_attr_err("admission_id");
                json_free(rows); json_free(store); json_free(ref); json_free(actor);
                json_free(sid); json_free(authority);
                return e;
            }
            json_t *cargs = ri3_arg3("actor", actor, "ref", ref, "admission_id", aid);
            json_t *cr = ri3_call(c, "authority.cancel_queued", cargs);
            if (ri3_is_raise(cr)) {
                if (!ri3_is_refusal(cr)) {
                    json_free(rows); json_free(store); json_free(ref); json_free(actor);
                    json_free(sid); json_free(authority);
                    return cr;
                }
                json_free(cr);   /* suppress(AuthorityRefusal) */
            } else {
                json_free(cr);
            }
        }
    }
    json_t *handle = ri3_call(c, "authority._handle", ri3_arg1("ref", ref));
    if (ri3_is_raise(handle)) {
        json_free(rows); json_free(store); json_free(ref); json_free(actor);
        json_free(sid); json_free(authority);
        return handle;
    }
    json_t *hstate = (handle && handle->type == JSON_OBJECT)
        ? json_obj_get(handle, "execution_state") : NULL;
    if (!hstate) {
        json_t *e = ri3_field_err(handle, "execution_state");
        json_free(handle); json_free(rows); json_free(store); json_free(ref);
        json_free(actor); json_free(sid); json_free(authority);
        return e;
    }
    if (ri3_str_eq(hstate, "running")) {
        json_t *gen = json_obj_get(handle, "execution_generation");
        if (!gen) {
            json_t *e = ri3_ns_attr_err("execution_generation");
            json_free(handle); json_free(rows); json_free(store); json_free(ref);
            json_free(actor); json_free(sid); json_free(authority);
            return e;
        }
        json_t *iargs = ri3_arg3("actor", actor, "ref", ref, "generation", gen);
        json_t *ir = ri3_call(c, "authority.interrupt", iargs);
        if (ri3_is_raise(ir)) {
            if (!ri3_is_refusal(ir)) {
                json_free(handle); json_free(rows); json_free(store); json_free(ref);
                json_free(actor); json_free(sid); json_free(authority);
                return ir;
            }
            json_free(ir);
        } else {
            json_free(ir);
        }
    }
    json_free(handle);
    json_free(rows);
    json_free(store);
    json_free(ref);
    json_free(actor);
    json_free(sid);
    json_free(authority);
    return json_null();
}

/* PoP: ri3_hm_handle_running_session_message @ gateway/run_inbound.py:_hm_handle_running_session_message */
static json_t *ri3_hm_handle_running_session_message(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");

    json_t *internal = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "internal") : NULL;
    if (ri3_truthy(internal)) {
        json_t *cmd = ri3_ev_command(c);
        RI3_CHK(cmd);
        bool has_cmd = ri3_truthy(cmd);
        json_free(cmd);
        if (!has_cmd) {
            json_t *q = ri3_call(c, "self._queue_or_replace_pending_event",
                                 ri3_arg2("key", json_string(key), "event", ev));
            RI3_CHK(q);
            json_free(q);
            return json_null();
        }
    }
    json_t *bs = ri3_call(c, "self._hm_busy_slash_or_photo",
                          ri3_arg3("event", ev, "source", src, "key", json_string(key)));
    RI3_CHK(bs);
    json_t *parts[2];
    if (!ri3_unpack2(bs, parts)) {
        json_t *e = parts[0];
        json_free(bs);
        return e;
    }
    bool handled = ri3_truthy(parts[0]);
    json_t *result = json_copy(parts[1]);
    json_free(bs);
    if (!handled) {
        json_free(result);
        json_t *sk = ri3_call(c, "self._busy_skill_turn",
                              ri3_arg3("event", ev, "source", src, "key", json_string(key)));
        RI3_CHK(sk);
        if (!ri3_unpack2(sk, parts)) {
            json_t *e = parts[0];
            json_free(sk);
            return e;
        }
        handled = ri3_truthy(parts[0]);
        result = json_copy(parts[1]);
        json_free(sk);
    }
    if (handled) return result;
    json_free(result);

    json_t *take = ri3_call(c, "self._clarify_takeover",
                            ri3_arg2("event", ev, "key", json_string(key)));
    RI3_CHK(take);
    bool takeover = ri3_truthy(take);
    json_free(take);
    const char *mode = "interrupt";
    json_t *mode_node = NULL;
    if (!takeover) {
        mode_node = ri3_call(c, "self._effective_busy_input_mode", ri3_arg1("source", src));
        RI3_CHK(mode_node);
        mode = ri3_str(mode_node) ? ri3_str(mode_node) : "";
    }
    json_t *gargs = ri3_arg3("event", ev, "source", src, "key", json_string(key));
    json_set_hdr(gargs, "mode", mode);
    json_t *grace = ri3_call(c, "self._hm_busy_telegram_grace_queue", gargs);
    if (ri3_is_raise(grace)) { json_free(mode_node); return grace; }
    bool queued = ri3_truthy(grace);
    json_free(grace);
    if (queued) { json_free(mode_node); return json_null(); }

    json_t *ra = ri3_peek_session_state(c, key);
    json_t *agent = NULL;
    if (ra) {
        json_t *turn = json_obj_get(ra, "turn");
        if (!turn || turn->type != JSON_OBJECT) { json_free(mode_node); return ri3_state_attr_err("turn"); }
        agent = json_obj_get(turn, "agent");
        if (!agent) { json_free(mode_node); return ri3_state_attr_err("agent"); }
    }
    if (ri3_str_eq(agent, RI3_SENTINEL)) {
        json_t *cmd = ri3_ev_command(c);
        if (ri3_is_raise(cmd)) { json_free(mode_node); return cmd; }
        bool stop = ri3_str_eq(cmd, "stop");
        json_free(cmd);
        if (stop) {
            json_t *cp = ri3_call(c, "cancel_preagent_setup", ri3_args_str("key", key));
            if (ri3_is_raise(cp)) { json_free(mode_node); return cp; }
            json_free(cp);
            json_t *rel = ri3_call(c, "self._release_running_agent_state", ri3_args_str("key", key));
            if (ri3_is_raise(rel)) { json_free(mode_node); return rel; }
            json_free(rel);
            json_t *txt = ri3_t(c, "gateway.stop.force_stopped_pending");
            if (ri3_is_raise(txt)) { json_free(mode_node); return txt; }
            /* EphemeralReply(str): null text -> str(None) == "None" */
            const char *s = ri3_str(txt);
            json_t *out = json_string(s ? s : (ri3_is_null(txt) ? "None" : ""));
            json_free(txt);
            json_free(mode_node);
            return out;
        }
        json_t *margs = ri3_arg3("source", src, "key", json_string(key), "event", ev);
        json_set(margs, "merge_text", json_bool(true));
        json_t *mg = ri3_call(c, "self._hm_merge_pending_for_source", margs);
        if (ri3_is_raise(mg)) { json_free(mode_node); return mg; }
        json_free(mg);
        json_t *notice = ri3_call(c, "pending_setup_busy_notice",
                                  ri3_arg2("key", json_string(key), "source", src));
        if (ri3_is_raise(notice)) { json_free(mode_node); return notice; }
        json_free(mode_node);
        return notice;
    }
    json_t *draining = json_obj_get(c->state, "_draining");
    if (ri3_truthy(draining)) {
        json_t *qdd = ri3_call(c, "self._queue_during_drain_enabled",
                               ri3_args_str("mode", mode));
        if (ri3_is_raise(qdd)) { json_free(mode_node); return qdd; }
        bool q = ri3_truthy(qdd);
        json_free(qdd);
        if (q) {
            json_t *qe = ri3_call(c, "self._queue_or_replace_pending_event",
                                  ri3_arg2("key", json_string(key), "event", ev));
            if (ri3_is_raise(qe)) { json_free(mode_node); return qe; }
            json_free(qe);
        }
        json_t *gerund = ri3_call(c, "self._status_action_gerund", json_object());
        if (ri3_is_raise(gerund)) { json_free(mode_node); return gerund; }
        json_t *targs = ri3_args_str("key", q ? "gateway.busy.drain_queued"
                                               : "gateway.busy.drain_rejected");
        json_set(targs, "action", gerund);
        json_t *txt = ri3_call(c, "t", targs);
        if (ri3_is_raise(txt)) { json_free(mode_node); return txt; }
        json_free(mode_node);
        return txt;
    }
    if (strcmp(mode, "queue") == 0) {
        json_t *qe = ri3_call(c, "self._queue_or_replace_pending_event",
                              ri3_arg2("key", json_string(key), "event", ev));
        if (ri3_is_raise(qe)) { json_free(mode_node); return qe; }
        json_free(qe);
        json_free(mode_node);
        return json_null();
    }
    if (strcmp(mode, "steer") == 0) {
        json_t *st = ri3_call(c, "self._hm_busy_steer",
                              ri3_arg3("event", ev, "agent", agent, "key", json_string(key)));
        if (ri3_is_raise(st)) { json_free(mode_node); return st; }
        json_free(st);
        json_free(mode_node);
        return json_null();
    }
    json_t *sub = ri3_call(c, "self._agent_has_active_subagents", ri3_arg1("agent", agent));
    if (ri3_is_raise(sub)) { json_free(mode_node); return sub; }
    bool demote = ri3_truthy(sub);
    json_free(sub);
    if (!demote) {
        json_t *comp = ri3_call(c, "self._session_has_compression_in_flight",
                                ri3_args_str("key", key));
        if (ri3_is_raise(comp)) { json_free(mode_node); return comp; }
        demote = ri3_truthy(comp);
        json_free(comp);
    }
    if (!demote) {
        /* live positional order (event, source, running_agent, key) */
        json_t *iargs = ri3_arg2("event", ev, "source", src);
        json_set(iargs, "agent", agent ? json_copy(agent) : json_null());
        json_set_hdr(iargs, "key", key);
        json_t *ir = ri3_call(c, "self._hm_busy_interrupt", iargs);
        if (ri3_is_raise(ir)) { json_free(mode_node); return ir; }
        json_free(ir);
        json_free(mode_node);
        return json_null();
    }
    json_t *qe = ri3_call(c, "self._queue_or_replace_pending_event",
                          ri3_arg2("key", json_string(key), "event", ev));
    if (ri3_is_raise(qe)) { json_free(mode_node); return qe; }
    json_free(qe);
    json_free(mode_node);
    return json_null();
}

/* PoP: ri3_prefix_inbound_sender_context @ gateway/run_inbound.py:_prefix_inbound_sender_context */
static json_t *ri3_prefix_inbound_sender_context(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    json_t *text = json_obj_get(args, "message_text");
    json_t *cfg = json_obj_get(c->state, "config");
    json_t *grp = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "group_sessions_per_user") : NULL;
    json_t *thr = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "thread_sessions_per_user") : NULL;
    json_t *largs = ri3_arg2("source", src, "text", text);
    json_set(largs, "group_sessions_per_user", grp ? json_copy(grp) : json_bool(true));
    json_set(largs, "thread_sessions_per_user", thr ? json_copy(thr) : json_bool(false));
    json_t *labeled = ri3_call(c, "label_shared_sender", largs);
    RI3_CHK(labeled);
    json_t *cc = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "channel_context") : NULL;
    if (ri3_truthy(cc)) {
        const char *ccs = ri3_str(cc) ? ri3_str(cc) : "";
        const char *ls = ri3_str(labeled) ? ri3_str(labeled) : "";
        size_t n = strlen(ccs) + strlen(ls) + 32;
        char *buf = malloc(n);
        if (buf) snprintf(buf, n, "%s\n\n[New message]\n%s", ccs, ls);
        json_free(labeled);
        labeled = json_string(buf ? buf : "");
        free(buf);
    }
    return labeled;
}

/* PoP: ri3_classify_inbound_media @ gateway/run_inbound.py:_classify_inbound_media */
static json_t *ri3_classify_inbound_media(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *pending = json_obj_get(args, "pending_stt_prepared");
    bool stt_prepared = ri3_truthy(pending);
    if (!ev || ev->type != JSON_OBJECT) {
        return ri3_attr_err(ev, "media_urls");
    }
    json_t *urls = json_obj_get(ev, "media_urls");
    if (!urls) return ri3_state_attr_err("media_urls");
    json_t *types = json_obj_get(ev, "media_types");
    json_t *mt = json_obj_get(ev, "message_type");
    const char *mt_s = ri3_str(mt) ? ri3_str(mt) : "";
    json_t *images = json_array(), *audios = json_array();
    json_t *afiles = json_array(), *videos = json_array();
    json_t *err = NULL;
    size_t n = (urls && urls->type == JSON_ARRAY) ? json_len(urls) : 0;
    size_t ntypes = (types && types->type == JSON_ARRAY) ? json_len(types) : 0;
    /* live reads event.media_types (len + index) on the first iteration,
     * before the is_image seam; absent / non-list carriers raise there. */
    if (n > 0) {
        if (!types) {
            json_free(images); json_free(audios); json_free(afiles); json_free(videos);
            return ri3_state_attr_err("media_types");
        }
        if (types->type != JSON_ARRAY) {
            char m[128];
            snprintf(m, sizeof m, "object of type '%s' has no len()", ri3_py_type(types));
            json_free(images); json_free(audios); json_free(afiles); json_free(videos);
            return ri3_err("TypeError", m);
        }
    }
    for (size_t i = 0; i < n && !err; i++) {
        json_t *path = json_get(urls, i);
        json_t *mtype = (i < ntypes) ? json_get(types, i) : NULL;
        const char *mtype_s = ri3_str(mtype);
        json_t *iargs = ri3_arg1("event", ev);
        json_set(iargs, "i", json_number((double)i));
        json_t *is_img = ri3_call(c, "event_media_is_image", iargs);
        if (ri3_is_raise(is_img)) { err = is_img; break; }
        if (ri3_truthy(is_img)) json_append(images, json_copy(path));
        json_free(is_img);
        iargs = ri3_arg1("event", ev);
        json_set(iargs, "i", json_number((double)i));
        json_t *is_aud = ri3_call(c, "event_media_is_audio", iargs);
        if (ri3_is_raise(is_aud)) { err = is_aud; break; }
        if (ri3_truthy(is_aud)) {
            if (strcmp(mt_s, "audio") == 0 || strcmp(mt_s, "document") == 0) {
                json_append(afiles, json_copy(path));
            } else if (!stt_prepared) {
                iargs = ri3_arg1("event", ev);
                json_set(iargs, "i", json_number((double)i));
                json_t *is_stt = ri3_call(c, "event_media_is_stt_input", iargs);
                if (ri3_is_raise(is_stt)) { err = is_stt; break; }
                if (ri3_truthy(is_stt)) json_append(audios, json_copy(path));
                json_free(is_stt);
            }
        }
        json_free(is_aud);
        /* mtype.startswith("video/") or (not mtype and message_type == VIDEO).
         * A present non-str mtype (incl null) raises on .startswith; an
         * ABSENT index defaults to "" (no raise). */
        if (i < ntypes && !mtype_s) {
            err = ri3_attr_err(mtype, "startswith");
            break;
        }
        bool is_video = (mtype_s && ri3_startswith(mtype_s, "video/")) ||
            (!ri3_truthy(mtype) && strcmp(mt_s, "video") == 0);
        if (is_video) json_append(videos, json_copy(path));
    }
    if (err) {
        json_free(images); json_free(audios); json_free(afiles); json_free(videos);
        return err;
    }
    json_t *out = json_array();
    json_append(out, images);
    json_append(out, audios);
    json_append(out, afiles);
    json_append(out, videos);
    return out;
}

/* PoP: ri3_enrich_inbound_images @ gateway/run_inbound.py:_enrich_inbound_images */
static json_t *ri3_enrich_inbound_images(ri3_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "session_key", "");
    json_t *text = json_obj_get(args, "message_text");
    json_t *paths = json_obj_get(args, "image_paths");

    json_t *mode = ri3_call(c, "self._decide_image_input_mode",
                            ri3_arg2("source", src, "session_key", json_string(key)));
    RI3_CHK(mode);
    if (ri3_str_eq(mode, "native")) {
        json_free(mode);
        json_t *node = ri3_session_state(c, key);
        json_t *persistent = json_obj_get(node, "persistent");
        if (!persistent || persistent->type != JSON_OBJECT) {
            return ri3_state_attr_err("persistent");
        }
        json_set(persistent, "native_image_paths",
                 paths ? json_copy(paths) : json_array());
        return text ? json_copy(text) : json_null();
    }
    json_free(mode);

    /* try: resolve runtime pair; except Exception: vision_runtime = None */
    json_t *vision_runtime = NULL;
    json_t *rargs = ri3_arg2("source", src, "session_key", json_string(key));
    json_set(rargs, "user_config", json_null());
    json_t *pair = ri3_call(c, "self._resolve_session_agent_runtime", rargs);
    if (ri3_is_raise(pair)) {
        if (!ri3_is_exception(pair)) return pair;
        json_free(pair);
    } else {
        json_t *parts[2];
        if (!ri3_unpack2(pair, parts)) {
            json_t *e = parts[0];
            json_free(pair);
            if (!ri3_is_exception(e)) return e;
            json_free(e);
        } else {
            json_t *model = parts[0];
            json_t *kw = parts[1];
            if (ri3_truthy(kw) && kw->type != JSON_OBJECT) {
                char m[128];
                snprintf(m, sizeof m, "argument of type '%s' is not a mapping",
                         ri3_py_type(kw));
                json_t *e = ri3_err("TypeError", m);
                json_free(pair);
                if (!ri3_is_exception(e)) return e;
                json_free(e);
            } else {
                vision_runtime = json_object();
                if (kw && kw->type == JSON_OBJECT) {
                    for (size_t i = 0; i < kw->c.count; i++) {
                        json_set(vision_runtime, kw->c.keys[i],
                                 json_copy(kw->c.items[i]));
                    }
                }
                json_set(vision_runtime, "model", model ? json_copy(model) : json_null());
                json_free(pair);
            }
        }
    }

    /* with scoped_runtime_main(vision_runtime): return await enrich(...) */
    json_t *cm = ri3_call(c, "scoped_runtime_main", ri3_arg1("runtime", vision_runtime));
    json_free(vision_runtime);
    RI3_CHK(cm);
    json_free(cm);
    json_t *en = ri3_call(c, "scoped_runtime_main.__enter__", json_object());
    RI3_CHK(en);
    json_free(en);
    json_t *body = ri3_call(c, "self._enrich_message_with_vision",
                            ri3_arg2("text", text, "image_paths", paths));
    json_t *exc_proj = NULL;
    if (ri3_is_raise(body)) exc_proj = json_copy(body);
    json_t *xargs = json_object();
    json_set(xargs, "exc", exc_proj ? exc_proj : json_null());
    json_t *ex = ri3_call(c, "scoped_runtime_main.__exit__", xargs);
    if (ri3_is_raise(ex)) {
        if (body) json_free(body);
        return ex;
    }
    bool suppress = ri3_truthy(ex);
    json_free(ex);
    if (ri3_is_raise(body)) {
        if (suppress) { json_free(body); return json_null(); }
        return body;
    }
    return body;
}

/* PoP: ri3_echo_stt_transcripts @ gateway/run_inbound.py:_echo_stt_transcripts */
static json_t *ri3_echo_stt_transcripts(ri3_ctx_t *c, json_t *args) {
    json_t *adapter = json_obj_get(args, "adapter");
    json_t *src = json_obj_get(args, "source");
    json_t *transcripts = json_obj_get(args, "transcripts");
    json_t *metadata = json_obj_get(args, "metadata");

    json_t *allowed = ri3_call(c, "automatic_notices_allowed", ri3_arg1("source", src));
    RI3_CHK(allowed);
    bool ok = ri3_truthy(allowed);
    json_free(allowed);
    long long accepted = 0;
    if (ok && transcripts && transcripts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(transcripts); i++) {
            json_t *tx = json_get(transcripts, i);
            /* adapter.send resolves the OUTER attribute before the t()
             * argument seam fires: a null adapter raises AttributeError
             * (swallowed) with NO t event. */
            if (ri3_is_null(adapter)) {
                continue;
            }
            json_t *targs = ri3_args_str("key", "gateway.voice.transcript_echo_short");
            json_set(targs, "text", tx ? json_copy(tx) : json_null());
            json_t *echo = ri3_call(c, "t", targs);
            if (ri3_is_raise(echo)) {
                if (!ri3_is_exception(echo)) return echo;
                json_free(echo);
                continue;
            }
            json_t *chat = (src && src->type == JSON_OBJECT) ? json_obj_get(src, "chat_id") : NULL;
            json_t *sargs = ri3_arg2("chat_id", chat, "text", echo);
            json_set(sargs, "metadata", metadata ? json_copy(metadata) : json_null());
            json_free(echo);
            json_t *result = ri3_call(c, "adapter.send", sargs);
            if (ri3_is_raise(result)) {
                if (!ri3_is_exception(result)) return result;
                json_free(result);
                continue;
            }
            json_t *succ = (result && result->type == JSON_OBJECT)
                ? json_obj_get(result, "success") : NULL;
            if (ri3_truthy(succ)) accepted++;
            json_free(result);
        }
    } else if (ok) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object is not iterable", ri3_py_type(transcripts));
        return ri3_err("TypeError", m);
    }
    return json_number((double)accepted);
}

/* PoP: ri3_inbound_attachment_display_name @ gateway/run_inbound.py:_inbound_attachment_display_name */
static json_t *ri3_inbound_attachment_display_name(ri3_ctx_t *c, json_t *args) {
    const char *path = json_get_str(args, "path", "");
    json_t *on = json_obj_get(args, "original_name");
    const char *original = ri3_str(on) ? ri3_str(on) : "";

    json_t *name = ri3_call(c, "clean_attachment_filename", ri3_args_str("name", original));
    RI3_CHK(name);
    const char *name_s = ri3_str(name);
    bool have_name = ri3_truthy(name);
    const char *base = ri3_basename(path);
    char *display = NULL;
    if (!have_name && ri3_startswith(base, RI3_RELAY_PREFIX)) {
        display = ri3_strdup(RI3_UNKNOWN_NAME);
    } else if (!have_name) {
        display = ri3_cache_name(base);
    } else {
        display = ri3_sanitize_name(name_s ? name_s : "");
    }
    json_free(name);
    json_t *agent_path = ri3_call(c, "to_agent_visible_cache_path", ri3_args_str("path", path));
    if (ri3_is_raise(agent_path)) { free(display); return agent_path; }
    json_t *out = ri3_pair(json_string(display ? display : ""), agent_path);
    free(display);
    json_free(agent_path);
    return out;
}

/* The live media-file note literal (U+2014 em dash rides the live text). */
static char *ri3_media_note(const char *kind, const char *display,
                            const char *agent_path, const char *noun,
                            const char *verb, const char *tool,
                            const char *message_text) {
    const char *fmt =
        "[The user sent %s: '%s'. "
        "It is saved at: %s. "
        "Its content is not inlined here. If the user's request involves "
        "what the %s contains, %s it yourself \xe2\x80\x94 for "
        "example by passing the path to %s \xe2\x80\x94 "
        "instead of asking the user to describe it. Only ask what to do "
        "with it if their intent is genuinely unclear.]"
        "\n\n%s";
    size_t n = strlen(kind) + strlen(display) + strlen(agent_path) +
        strlen(noun) + strlen(verb) + strlen(tool) + strlen(message_text) + 512;
    char *buf = malloc(n);
    if (buf) snprintf(buf, n, fmt, kind, display, agent_path, noun, verb, tool, message_text);
    return buf;
}

/* PoP: ri3_prepend_inbound_media_file_notes @ gateway/run_inbound.py:_prepend_inbound_media_file_notes */
static json_t *ri3_prepend_inbound_media_file_notes(ri3_ctx_t *c, json_t *args) {
    json_t *text = json_obj_get(args, "message_text");
    json_t *afiles = json_obj_get(args, "audio_file_paths");
    json_t *videos = json_obj_get(args, "video_paths");
    json_t *names = json_obj_get(args, "original_names");
    char *cur = ri3_strdup(ri3_str(text) ? ri3_str(text) : "");
    static const struct {
        const char *kind, *noun, *verb, *tool;
        const char *paths_key;
    } KINDS[2] = {
        {"an audio file attachment", "audio", "transcribe or process",
         "a transcription or media tool", "audio_file_paths"},
        {"a video attachment", "video", "inspect or process",
         "a video analysis or media tool", "video_paths"},
    };
    json_t *lists[2] = {afiles, videos};
    for (int k = 0; k < 2; k++) {
        json_t *paths = lists[k];
        if (!paths || paths->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < json_len(paths); i++) {
            json_t *path = json_get(paths, i);
            const char *path_s = ri3_str(path) ? ri3_str(path) : "";
            json_t *on = (names && names->type == JSON_OBJECT)
                ? json_obj_get(names, path_s) : NULL;
            json_t *dargs = json_object();
            json_set_hdr(dargs, "path", path_s);
            json_set_hdr(dargs, "original_name", ri3_str(on) ? ri3_str(on) : "");
            json_t *pair = ri3_inbound_attachment_display_name(c, dargs);
            json_free(dargs);
            if (ri3_is_raise(pair)) { free(cur); return pair; }
            json_t *parts[2];
            if (!ri3_unpack2(pair, parts)) {
                json_t *e = parts[0];
                json_free(pair);
                free(cur);
                return e;
            }
            const char *display = ri3_str(parts[0]) ? ri3_str(parts[0]) : "";
            const char *agent_path = ri3_str(parts[1]) ? ri3_str(parts[1]) : "";
            char *note = ri3_media_note(KINDS[k].kind, display, agent_path,
                                        KINDS[k].noun, KINDS[k].verb, KINDS[k].tool, cur);
            json_free(pair);
            free(cur);
            cur = note ? note : ri3_strdup("");
        }
    }
    json_t *out = json_string(cur);
    free(cur);
    return out;
}

/* PoP: ri3_prepend_inbound_document_notes @ gateway/run_inbound.py:_prepend_inbound_document_notes */
static json_t *ri3_prepend_inbound_document_notes(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *text = json_obj_get(args, "message_text");
    if (!ev || ev->type != JSON_OBJECT) {
        return ri3_attr_err(ev, "media_urls");
    }
    json_t *urls = json_obj_get(ev, "media_urls");
    if (!urls) return ri3_state_attr_err("media_urls");
    if (!ri3_truthy(urls)) {
        return text ? json_copy(text) : json_null();
    }
    if (urls->type != JSON_ARRAY) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object is not iterable", ri3_py_type(urls));
        return ri3_err("TypeError", m);
    }
    json_t *types = json_obj_get(ev, "media_types");
    json_t *inline_flags = json_obj_get(ev, "media_text_inlined");
    if (ri3_is_null(inline_flags)) inline_flags = NULL;
    json_t *names = json_obj_get(ev, "media_filenames");
    if (ri3_is_null(names)) names = NULL;
    size_t ntypes = (types && types->type == JSON_ARRAY) ? json_len(types) : 0;
    size_t nflags = (inline_flags && inline_flags->type == JSON_ARRAY) ? json_len(inline_flags) : 0;
    size_t nnames = (names && names->type == JSON_ARRAY) ? json_len(names) : 0;
    char *cur = ri3_strdup(ri3_str(text) ? ri3_str(text) : "");
    static const char *TEXT_EXTS[] = {
        ".txt", ".md", ".csv", ".log", ".json", ".xml",
        ".yaml", ".yml", ".toml", ".ini", ".cfg",
    };
    for (size_t i = 0; i < json_len(urls); i++) {
        json_t *path = json_get(urls, i);
        const char *path_s = ri3_str(path) ? ri3_str(path) : "";
        /* any(f(event, i) for f in (is_image, is_audio, is_video)) */
        bool media = false;
        const char *fnames[3] = {"event_media_is_image", "event_media_is_audio",
                                 "event_media_is_video"};
        for (int f = 0; f < 3 && !media; f++) {
            json_t *fargs = ri3_arg1("event", ev);
            json_set(fargs, "i", json_number((double)i));
            json_t *r = ri3_call(c, fnames[f], fargs);
            if (ri3_is_raise(r)) { free(cur); return r; }
            media = ri3_truthy(r);
            json_free(r);
        }
        if (media) continue;
        /* live reads event.media_types (len + index) only for non-media files */
        if (!types) { free(cur); return ri3_state_attr_err("media_types"); }
        if (types->type != JSON_ARRAY) {
            char m[128];
            snprintf(m, sizeof m, "object of type '%s' has no len()", ri3_py_type(types));
            free(cur);
            return ri3_err("TypeError", m);
        }
        json_t *mtype = (i < ntypes) ? json_get(types, i) : NULL;
        const char *mtype_s = ri3_str(mtype);
        char *mtype_buf = NULL;
        /* mtype in {"", "application/octet-stream"} — a null mtype is NOT in
         * the set (membership, no .startswith here) and rides to the note. */
        if (mtype_s && (strcmp(mtype_s, "") == 0 ||
                        strcmp(mtype_s, "application/octet-stream") == 0)) {
            char *ext = ri3_splitext(path_s);
            for (char *q = ext; *q; q++) *q = (char)tolower((unsigned char)*q);
            bool is_text = false;
            for (size_t e = 0; e < sizeof(TEXT_EXTS) / sizeof(TEXT_EXTS[0]); e++)
                if (strcmp(ext, TEXT_EXTS[e]) == 0) { is_text = true; break; }
            free(ext);
            if (is_text) {
                mtype_buf = ri3_strdup("text/plain");
            } else {
                json_t *gt = ri3_call(c, "mimetypes.guess_type", ri3_args_str("path", path_s));
                if (ri3_is_raise(gt)) { free(cur); return gt; }
                json_t *g0 = (gt && gt->type == JSON_ARRAY && json_len(gt) > 0)
                    ? json_get(gt, 0) : NULL;
                if (ri3_is_null(gt)) {
                    json_free(gt);
                    free(cur);
                    return ri3_err("TypeError", "'NoneType' object is not subscriptable");
                }
                if (gt->type != JSON_ARRAY) {
                    json_t *e = ri3_attr_err(gt, "__getitem__");
                    json_free(gt);
                    free(cur);
                    return e;
                }
                const char *gs = ri3_str(g0);
                mtype_buf = ri3_strdup(ri3_truthy(g0) && gs ? gs : "application/octet-stream");
                json_free(gt);
            }
            mtype_s = mtype_buf;
        }
        /* display name via the composed ported sibling */
        json_t *on = (i < nnames) ? json_get(names, i) : NULL;
        json_t *dargs = json_object();
        json_set_hdr(dargs, "path", path_s);
        json_set_hdr(dargs, "original_name", ri3_str(on) ? ri3_str(on) : "");
        json_t *pair = ri3_inbound_attachment_display_name(c, dargs);
        json_free(dargs);
        if (ri3_is_raise(pair)) { free(mtype_buf); free(cur); return pair; }
        const char *display = ri3_str(json_get(pair, 0)) ? ri3_str(json_get(pair, 0)) : "";
        const char *agent_path = ri3_str(json_get(pair, 1)) ? ri3_str(json_get(pair, 1)) : "";
        json_t *flag = (i < nflags) ? json_get(inline_flags, i) : NULL;
        bool inlined = flag && flag->type == JSON_BOOL && flag->bool_val;
        json_t *nargs = json_object();
        json_set_hdr(nargs, "display_name", display);
        json_set_hdr(nargs, "agent_path", agent_path);
        if (mtype_s) json_set_hdr(nargs, "mtype", mtype_s);
        else json_set(nargs, "mtype", mtype ? json_copy(mtype) : json_null());
        json_set(nargs, "content_inlined", json_bool(inlined));
        json_t *note = ri3_call(c, "build_document_context_note", nargs);
        json_free(pair);
        free(mtype_buf);
        if (ri3_is_raise(note)) { free(cur); return note; }
        const char *note_s = ri3_str(note) ? ri3_str(note) : "";
        size_t n = strlen(note_s) + strlen(cur) + 4;
        char *buf = malloc(n);
        if (buf) snprintf(buf, n, "%s\n\n%s", note_s, cur);
        json_free(note);
        free(cur);
        cur = buf ? buf : ri3_strdup("");
    }
    json_t *out = json_string(cur);
    free(cur);
    return out;
}

/* PoP: ri3_prepend_inbound_reply_context @ gateway/run_inbound.py:_prepend_inbound_reply_context */
static json_t *ri3_prepend_inbound_reply_context(ri3_ctx_t *c, json_t *args) {
    json_t *ev = json_obj_get(args, "event");
    json_t *src = json_obj_get(args, "source");
    json_t *text = json_obj_get(args, "message_text");
    char *cur = ri3_strdup(ri3_str(text) ? ri3_str(text) : "");
    json_t *rt = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "reply_to_text") : NULL;
    if (ri3_truthy(rt)) {
        json_t *mid = json_obj_get(ev, "reply_to_message_id");
        if (!mid) { free(cur); return ri3_state_attr_err("reply_to_message_id"); }
        if (ri3_truthy(mid)) {
            json_t *own = json_obj_get(ev, "reply_to_is_own_message");
            if (!src || src->type != JSON_OBJECT) {
                free(cur);
                return ri3_attr_err(src, "platform");
            }
            json_t *plat = json_obj_get(src, "platform");
            if (!plat) { free(cur); return ri3_ns_attr_err("platform"); }
            json_t *chat = json_obj_get(src, "chat_id");
            if (!chat) { free(cur); return ri3_ns_attr_err("chat_id"); }
            json_t *replied = json_array();
            json_append(replied, json_copy(plat));
            json_append(replied, json_copy(chat));
            json_append(replied, json_copy(mid));
            json_t *pargs = ri3_arg1("text", rt);
            json_set(pargs, "own_message", json_bool(ri3_truthy(own)));
            json_set(pargs, "replied_to", replied);
            json_t *ptr = ri3_call(c, "reply_pointer", pargs);
            if (ri3_is_raise(ptr)) { free(cur); return ptr; }
            const char *ptr_s = ri3_str(ptr) ? ri3_str(ptr) : "";
            size_t n = strlen(ptr_s) + strlen(cur) + 4;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "%s\n\n%s", ptr_s, cur);
            json_free(ptr);
            free(cur);
            cur = buf ? buf : ri3_strdup("");
        }
    }
    /* Discord triggering note (outermost prefix) */
    json_t *plat = (src && src->type == JSON_OBJECT) ? json_obj_get(src, "platform") : NULL;
    json_t *mid = (ev && ev->type == JSON_OBJECT) ? json_obj_get(ev, "message_id") : NULL;
    if (!ri3_is_null(src) && ri3_str_eq(plat, "discord") && ri3_truthy(mid)) {
        json_t *loaded = ri3_call(c, "_discord_tools_loaded", json_object());
        if (ri3_is_raise(loaded)) { free(cur); return loaded; }
        bool yes = ri3_truthy(loaded);
        json_free(loaded);
        if (yes) {
            json_t *note = ri3_call(c, "discord_triggering_note", ri3_arg1("message_id", mid));
            if (ri3_is_raise(note)) { free(cur); return note; }
            const char *note_s = ri3_str(note) ? ri3_str(note) : "";
            size_t n = strlen(note_s) + strlen(cur) + 4;
            char *buf = malloc(n);
            if (buf) snprintf(buf, n, "%s\n\n%s", note_s, cur);
            json_free(note);
            free(cur);
            cur = buf ? buf : ri3_strdup("");
        }
    }
    json_t *out = json_string(cur);
    free(cur);
    return out;
}

/* PoP: ri3_inbound_model_context_length @ gateway/run_inbound.py:_inbound_model_context_length */
static json_t *ri3_inbound_model_context_length(ri3_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "session_key", "");

    json_t *config_ctx = NULL;          /* owned number or NULL (None) */
    json_t *cfg = NULL;                 /* owned */
    json_t *model_cfg = json_object();  /* owned; {} default */
    json_t *providers = json_array();   /* owned; [] default */
    json_t *model = NULL;               /* owned */
    json_t *runtime = NULL;             /* owned */
    json_t *err = NULL;

    /* with suppress(Exception): config load + pin read + custom providers */
    cfg = ri3_call(c, "_load_gateway_config", json_object());
    if (ri3_is_raise(cfg)) {
        if (!ri3_is_exception(cfg)) { err = cfg; cfg = NULL; goto cleanup; }
        json_free(cfg); cfg = NULL;
        goto after_cfg;
    }
    if (ri3_is_null(cfg) || cfg->type != JSON_OBJECT) {
        /* _msg_cfg.get("model", {}) on a non-dict carrier */
        err = ri3_attr_err(cfg, "get");
        json_free(cfg); cfg = NULL;
        if (!ri3_is_exception(err)) goto cleanup;
        json_free(err); err = NULL;
        goto after_cfg;
    }
    {
        json_t *mc = json_obj_get(cfg, "model");
        json_free(model_cfg);
        model_cfg = mc ? json_copy(mc) : json_object();
    }
    if (model_cfg->type == JSON_OBJECT) {
        json_t *raw_ctx = json_obj_get(model_cfg, "context_length");
        if (!ri3_is_null(raw_ctx)) {
            json_t *ie = NULL;
            json_t *iv = ri3_int_coerce(raw_ctx, &ie);
            if (ie) {
                if (!ri3_is_exception(ie)) { err = ie; goto cleanup; }
                json_free(ie);
                goto after_cfg;
            }
            config_ctx = iv;
        }
    }
    {
        json_t *cp = ri3_call(c, "get_compatible_custom_providers", ri3_arg1("cfg", cfg));
        if (ri3_is_raise(cp)) {
            if (!ri3_is_exception(cp)) { err = cp; goto cleanup; }
            json_free(cp);
            /* except Exception: fall back to cfg.custom_providers or [] */
            json_t *fallback = json_obj_get(cfg, "custom_providers");
            json_free(providers);
            providers = ri3_truthy(fallback) ? json_copy(fallback) : json_array();
        } else {
            json_free(providers);
            providers = cp;
        }
    }
after_cfg:
    ;
    json_t *rargs = ri3_arg2("source", src, "session_key", json_string(key));
    json_set(rargs, "user_config", cfg ? json_copy(cfg) : json_null());
    json_t *pair = ri3_call(c, "self._resolve_session_agent_runtime", rargs);
    if (ri3_is_raise(pair)) { err = pair; goto cleanup; }
    json_t *parts[2];
    if (!ri3_unpack2(pair, parts)) {
        err = parts[0];
        json_free(pair);
        goto cleanup;
    }
    model = json_copy(parts[0]);
    runtime = json_copy(parts[1]);
    json_free(pair);
    if (ri3_is_null(runtime) || runtime->type != JSON_OBJECT) {
        err = ri3_attr_err(runtime, "get");
        goto cleanup;
    }
    json_t *base_url_node = json_obj_get(runtime, "base_url");
    const char *base_url = ri3_truthy(base_url_node) ? ri3_str(base_url_node) : NULL;
    if (!base_url) base_url = "";
    json_t *configured_model;
    if (model_cfg->type == JSON_OBJECT) {
        json_t *dflt = json_obj_get(model_cfg, "default");
        json_t *mdl = json_obj_get(model_cfg, "model");
        configured_model = ri3_truthy(dflt) ? dflt : mdl;
    } else {
        configured_model = model_cfg;
    }
    if (!ri3_val_eq(model, configured_model)) {
        json_free(config_ctx);
        config_ctx = NULL;
    }
    if (config_ctx) {
        json_t *cbu = json_obj_get(model_cfg, "base_url");
        json_t *cprov = json_obj_get(model_cfg, "provider");
        json_t *rprov = json_obj_get(runtime, "provider");
        json_t *sargs = json_object();
        json_set(sargs, "model", json_null());
        json_set(sargs, "configured_model", json_null());
        json_set(sargs, "config_base_url", cbu ? json_copy(cbu) : json_null());
        json_set_hdr(sargs, "base_url", base_url);
        json_set(sargs, "config_provider", cprov ? json_copy(cprov) : json_null());
        json_set(sargs, "provider", rprov ? json_copy(rprov) : json_null());
        json_t *clear = ri3_call(c, "should_clear_context_pin_async", sargs);
        if (ri3_is_raise(clear)) {
            if (!ri3_is_exception(clear)) { err = clear; goto cleanup; }
            json_free(clear);
            json_free(config_ctx);
            config_ctx = NULL;
        } else {
            if (ri3_truthy(clear)) {
                json_free(config_ctx);
                config_ctx = NULL;
            }
            json_free(clear);
        }
    }
    if (ri3_truthy(providers) && base_url[0]) {
        /* with suppress(Exception): custom-provider limit wins when truthy */
        json_t *gargs = json_object();
        json_set(gargs, "model", json_copy(model));
        json_set_hdr(gargs, "base_url", base_url);
        json_set(gargs, "custom_providers", json_copy(providers));
        json_t *gcl = ri3_call(c, "get_custom_provider_context_length", gargs);
        if (ri3_is_raise(gcl)) {
            if (!ri3_is_exception(gcl)) { err = gcl; goto cleanup; }
            json_free(gcl);
        } else {
            if (ri3_truthy(gcl)) {
                json_free(config_ctx);
                config_ctx = gcl;
            } else {
                json_free(gcl);
            }
        }
    }
    {
        json_t *api_key = json_obj_get(runtime, "api_key");
        json_t *rprov = json_obj_get(runtime, "provider");
        json_t *fargs = json_object();
        json_set(fargs, "model", model);
        model = NULL;
        json_set_hdr(fargs, "base_url", base_url);
        json_set_hdr(fargs, "api_key",
                     ri3_truthy(api_key) ? (ri3_str(api_key) ? ri3_str(api_key) : "") : "");
        json_set(fargs, "config_context_length", config_ctx ? config_ctx : json_null());
        config_ctx = NULL;
        json_set_hdr(fargs, "provider",
                     ri3_truthy(rprov) ? (ri3_str(rprov) ? ri3_str(rprov) : "") : "");
        json_set(fargs, "custom_providers", providers);
        providers = NULL;
        json_t *ret = ri3_call(c, "get_model_context_length_async", fargs);
        json_free(runtime);
        json_free(cfg);
        json_free(model_cfg);
        return ret;
    }
cleanup:
    json_free(model);
    json_free(runtime);
    json_free(cfg);
    json_free(model_cfg);
    json_free(providers);
    json_free(config_ctx);
    return err;
}

/* PoP: ri3_expand_inbound_context_references @ gateway/run_inbound.py:_expand_inbound_context_references */
static json_t *ri3_expand_inbound_context_references(ri3_ctx_t *c, json_t *args) {
    json_t *src = json_obj_get(args, "source");
    const char *key = json_get_str(args, "session_key", "");
    json_t *text = json_obj_get(args, "message_text");

    json_t *rargs = ri3_arg2("source", src, "session_key", json_string(key));
    json_set(rargs, "text", text ? json_copy(text) : json_null());
    json_t *pair = ri3_call(c, "inbound_references", rargs);
    RI3_CHK(pair);
    json_t *parts[2];
    if (!ri3_unpack2(pair, parts)) {
        json_t *e = parts[0];
        json_free(pair);
        return e;
    }
    json_t *expanded = json_copy(parts[0]);
    json_t *refusal = parts[1];
    if (ri3_is_null(refusal)) {
        json_free(pair);
        return expanded;
    }
    json_free(expanded);
    json_t *allowed = ri3_call(c, "automatic_notices_allowed", ri3_arg1("source", src));
    if (ri3_is_raise(allowed)) { json_free(pair); return allowed; }
    bool ok = ri3_truthy(allowed);
    json_free(allowed);
    json_t *adapter = NULL;
    if (ok) {
        adapter = ri3_call(c, "self._delivery_adapter_for", ri3_arg1("source", src));
        if (ri3_is_raise(adapter)) { json_free(pair); return adapter; }
    }
    if (ri3_truthy(adapter)) {
        json_t *chat = (src && src->type == JSON_OBJECT) ? json_obj_get(src, "chat_id") : NULL;
        json_t *sargs = ri3_arg2("chat_id", chat, "text", refusal);
        json_set(sargs, "metadata", json_null());
        json_t *sent = ri3_call(c, "adapter.send", sargs);
        if (ri3_is_raise(sent)) {
            json_free(adapter); json_free(pair);
            return sent;
        }
        json_free(sent);
    }
    json_free(adapter);
    json_free(pair);
    return json_null();
}