/*
 * stream_dispatch_flow2.c — gateway/stream_dispatch.py FLOW remainder
 * (universal-seam convention, twin of gateway_run_flow2 / stream_consumer_flow).
 *
 * Scope / claim accounting (grep PoP, CRLF-aware, before porting):
 *   gateway/stream_dispatch.py defines exactly FOUR methods. Three are already
 *   CLAIMED by the legacy stub port src/gateway/stream_dispatch_ports.c
 *   (std_init @ __init__, std_dispatch @ dispatch, std_dispatch_inner @
 *   _dispatch). The single genuinely UNCLAIMED method is _dispatch_tool_call,
 *   which carries the one new PoP claim below. The already-claimed siblings
 *   dispatch / _dispatch are re-implemented here ONLY as demoted internal
 *   helpers (sd2_dispatch / sd2_dispatch_inner, NO PoP line — per the re-port
 *   rule a faithful copy of a claimed name must not re-claim it) so the FLOW
 *   routes every event type end-to-end and the differential oracle exercises
 *   the real exception-swallowing wrapper. __init__ is modeled as state setup
 *   (tool_mode or "all", preview_max_len default 40, _last_tool None).
 *
 * Every live collaborator call enters through sd2_seams_t.call(ctx,name,args):
 *   render_message_event   adapter.render_message_event(event, sink)
 *   format_tool_event      adapter.format_tool_event(event, mode=, preview_max_len=)
 *   _on_long_tool          self._on_long_tool(event)        (LongToolHint)
 *   _on_notice             self._on_notice(event)           (GatewayNotice)
 *   _enqueue_tool_line     self._enqueue_tool_line(line)    (tool chrome)
 *   logger.debug           logger.debug(fmt, exc_info=True) (dispatch swallow)
 * args is OWNED by sd2_call (freed after the borrowed call); results are NEW
 * json_t with NULL == Python None and {"__raise__":K,"msg":M} == a raised
 * exception. BaseException kinds (SystemExit / KeyboardInterrupt /
 * CancelledError / GeneratorExit) pass through every `except Exception` site;
 * the full builtin exception hierarchy table below drives that decision.
 * Fail-closed: the harness delivers a RuntimeError raise spec for an
 * unscripted (missing/empty) seam.
 *
 * Modeling notes (differential oracle tests/oracle_stream_dispatch_flow2.py):
 *  - Events travel as JSON objects tagged with "_type" (the dataclass name;
 *    "_type" avoids colliding with GatewayNotice's real "kind" field);
 *    the oracle materializes the real frozen dataclasses so isinstance works,
 *    and projects the event back to the fixture dict for seam args. Fixture
 *    event dicts carry every field explicitly (no dataclass-default drift).
 *  - Dispatcher STATE (in.state -> out.state) is mutable: _last_tool is the
 *    "new"-mode dedup token written by _dispatch_tool_call. tool_mode is
 *    normalized once (tool_mode or "all"). sink / _enqueue_tool_line /
 *    _on_long_tool / _on_notice presence is an `is None` IDENTITY test, so a
 *    present marker (any non-null node, incl. {}) means "callback installed";
 *    null means None. adapter is always present.
 *  - out.state echoes {tool_mode, preview_max_len, _last_tool} only (the
 *    scalar/string state; callback/sink markers are input-only, never mutated).
 *  - ToolCallFinished and unknown kinds match no _dispatch branch (no-op).
 *  - DOMAIN: sd2_dispatch_tool_call is driven only with ToolCallChunk events
 *    (its live contract — _dispatch isinstance-guards the call); a non-tool
 *    event read directly for .tool_name is out of domain. Exception-message
 *    domain is kinds whose str(e) == the scripted msg (ValueError / RuntimeError
 *    / TypeError / OSError / SystemExit / KeyboardInterrupt / CancelledError /
 *    GeneratorExit / custom Exception subclasses); KeyError-style repr-quoting
 *    (str(KeyError("m")) == "'m'") is outside the fixture corpus.
 *  - Integral floats normalize to ints on both sides; ASCII fixture domain.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "libjson/json.h"

typedef struct sd2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} sd2_seams_t;

/* ── universal-seam helpers ── */
static json_t *sd2_call(const sd2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool sd2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *sd2_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *sd2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ──
 * {name, parent}. A kind passes through `except Exception` iff walking its
 * parent chain reaches BaseException WITHOUT touching Exception. Unknown
 * kinds default to Exception-derived (the oracle's make_exc builds them as
 * type(kind,(Exception,))), so they are caught. asyncio.CancelledError is a
 * BaseException subclass (3.8+) and is included as a base-only leaf. */
typedef struct { const char *name; const char *parent; } sd2_exc_t;
static const sd2_exc_t sd2_exc_table[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"CancelledError", "BaseException"},
};
static const char *sd2_exc_parent(const char *name) {
    for (size_t i = 0; i < sizeof(sd2_exc_table) / sizeof(sd2_exc_table[0]); i++)
        if (strcmp(sd2_exc_table[i].name, name) == 0) return sd2_exc_table[i].parent;
    return "Exception";  /* unknown kind: oracle makes an Exception subclass */
}
/* True iff the kind is NOT an Exception subclass (passes `except Exception`). */
static bool sd2_is_base_exc(const char *kind) {
    const char *cur = kind;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return false;
        if (strcmp(cur, "BaseException") == 0) return true;
        cur = sd2_exc_parent(cur);
    }
    return false;
}

static bool sd2_truth(const json_t *v) {
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
static const char *sd2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool sd2_is_null_node(const json_t *v) { return !v || v->type == JSON_NULL; }
static json_t *sd2_args(void) { return json_object(); }
static void sd2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void sd2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void sd2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* logger.debug(fmt, exc_info=True) — the dispatch swallow handler shape. */
static json_t *sd2_log_exc_info(const sd2_seams_t *s, const char *level, const char *fmt) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    json_t *a = sd2_args();
    sd2_s(a, "fmt", fmt);
    json_set(a, "exc_info", json_bool(true));
    return sd2_call(s, name, a);
}

/* ── _dispatch_tool_call (the UNCLAIMED method) ── */
/* PoP: sd2_dispatch_tool_call @ gateway/stream_dispatch.py:_dispatch_tool_call */
static json_t *sd2_dispatch_tool_call(const sd2_seams_t *s, json_t *state, const json_t *event) {
    const char *tool_mode = json_get_str(state, "tool_mode", "all");
    const json_t *enq = json_obj_get(state, "_enqueue_tool_line");
    /* if self.tool_mode == "off" or self._enqueue_tool_line is None: return */
    if (strcmp(tool_mode, "off") == 0 || sd2_is_null_node(enq)) return NULL;
    const char *tool_name = json_get_str(event, "tool_name", "");
    /* if self.tool_mode == "new" and event.tool_name == self._last_tool: return */
    if (strcmp(tool_mode, "new") == 0) {
        const char *last = sd2_sval(json_obj_get(state, "_last_tool"));
        if (last && strcmp(tool_name, last) == 0) return NULL;
    }
    /* self._last_tool = event.tool_name */
    json_set(state, "_last_tool", json_string(tool_name));
    /* line = self.adapter.format_tool_event(event, mode=..., preview_max_len=...) */
    json_t *a = sd2_args();
    sd2_j(a, "event", event);
    sd2_s(a, "mode", tool_mode);
    sd2_n(a, "preview_max_len", json_get_num(state, "preview_max_len", 40));
    json_t *line = sd2_call(s, "format_tool_event", a);
    if (sd2_raised(line)) return line;
    /* if line:  self._enqueue_tool_line(line)   (None/"" == adapter ate it) */
    if (sd2_truth(line)) {
        json_t *ea = sd2_args();
        sd2_j(ea, "line", line);
        json_t *er = sd2_call(s, "_enqueue_tool_line", ea);
        json_free(line);
        if (sd2_raised(er)) return er;
        json_free(er);
        return NULL;
    }
    json_free(line);
    return NULL;
}

/* ── _dispatch (claimed sibling: demoted internal helper, NO PoP) ── */
static json_t *sd2_dispatch_inner(const sd2_seams_t *s, json_t *state, const json_t *event) {
    const char *kind = json_get_str(event, "_type", "");
    if (strcmp(kind, "MessageChunk") == 0 || strcmp(kind, "MessageStop") == 0
        || strcmp(kind, "Commentary") == 0) {
        /* if self.sink is not None: self.adapter.render_message_event(event, self.sink) */
        const json_t *sink = json_obj_get(state, "sink");
        if (!sd2_is_null_node(sink)) {
            json_t *a = sd2_args();
            sd2_j(a, "event", event);
            sd2_j(a, "sink", sink);
            json_t *r = sd2_call(s, "render_message_event", a);
            if (sd2_raised(r)) return r;
            json_free(r);
        }
        return NULL;
    } else if (strcmp(kind, "ToolCallChunk") == 0) {
        return sd2_dispatch_tool_call(s, state, event);
    } else if (strcmp(kind, "LongToolHint") == 0) {
        const json_t *cb = json_obj_get(state, "_on_long_tool");
        if (!sd2_is_null_node(cb)) {
            json_t *a = sd2_args();
            sd2_j(a, "event", event);
            json_t *r = sd2_call(s, "_on_long_tool", a);
            if (sd2_raised(r)) return r;
            json_free(r);
        }
        return NULL;
    } else if (strcmp(kind, "GatewayNotice") == 0) {
        const json_t *cb = json_obj_get(state, "_on_notice");
        if (!sd2_is_null_node(cb)) {
            json_t *a = sd2_args();
            sd2_j(a, "event", event);
            json_t *r = sd2_call(s, "_on_notice", a);
            if (sd2_raised(r)) return r;
            json_free(r);
        }
        return NULL;
    }
    /* ToolCallFinished / unknown kind: no branch matches (no-op). */
    return NULL;
}

/* ── dispatch (claimed sibling: demoted internal helper, NO PoP) ── */
static json_t *sd2_dispatch(const sd2_seams_t *s, json_t *state, const json_t *event) {
    /* try: self._dispatch(event) */
    json_t *r = sd2_dispatch_inner(s, state, event);
    if (sd2_raised(r)) {
        /* BaseException passes through `except Exception`. */
        if (sd2_is_base_exc(sd2_kind(r))) return r;
        /* except Exception: logger.debug("stream-event dispatch error", exc_info=True) */
        json_free(r);
        json_t *lr = sd2_log_exc_info(s, "debug", "stream-event dispatch error");
        if (sd2_raised(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(r);
    return NULL;
}