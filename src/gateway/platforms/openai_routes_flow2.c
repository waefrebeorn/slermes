/*
 * openai_routes_flow2.c — gateway/platforms/api_server_openai_routes.py
 * UNCLAIMED remainder FLOW (wave 2; LIVE authority:
 * C:/Users/eman5/hermes-agent/gateway/platforms/api_server_openai_routes.py).
 *
 * Ports the notice/item builders (_transformed_notice, _message_item,
 * _reasoning_item), the tool-payload trimmer (_trim_tool_items), the
 * turn-transcript cluster (_response_messages_turn_start_index,
 * _turn_transcript_messages, _turn_reasoning_text, _extract_output_items),
 * the chat admission identity leaves (_chat_request_id,
 * _authority_chat_kwargs), the async admission flows (_response_identity,
 * _implicit_chat_session, _run_idempotent, _prepare_sse_response,
 * _replay_sse_responses, _responses_durable_replay) and the
 * _ResponsesStream state/method cores (_item_id, envelope,
 * terminal_envelope, _history_with_user, _persist_snapshot,
 * _commit_terminal, write_event).
 *
 * The 18 flow1 siblings (openai_routes_flow.c) and the four route handlers
 * (api_server_adapter_chat.c / api_server_adapter_responses.c) are CLAIMED
 * and NOT re-ported: flow1-claimed leaves reached from here (_cap_text,
 * _settled, _responses_fingerprint_keys, _matching_replay) enter as SEAMS.
 * _response_messages_turn_start_index / _turn_transcript_messages /
 * _extract_output_items carry stale PoP claims against api_server.py (which
 * no longer defines them — they moved to this mixin module); the defining
 * module is api_server_openai_routes.py, so they are claimed HERE.
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * `name` identifies the live call (table below); `args` borrowed JSON object
 * (null-valued keys omitted on BOTH sides); result NEW json_t* (NULL == None,
 * never a JSON_NULL node); {"__raise__": cls, "msg": msg, ...} == exception.
 * FAIL-CLOSED: a MISSING script returns {"__raise__":"RuntimeError",
 * "msg":"no script for <name>"} (recorded as an event, then propagated);
 * a scripted null is a real None result. The harness records
 * {"ev":name,"args":args,"ret":result} per call in order;
 * tests/oracle_openai_routes_flow2.py drives LIVE Python with the same
 * scripts and compares ordered trace + ret + state. No I/O in the cores.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller
 * frees): NULL == None; a {"__raise__":cls,"msg":msg} object == a raised
 * exception (propagated or caught exactly as the live def does). Tuples are
 * JSON arrays; dict key order is insertion order (semantic). Stateful
 * _ResponsesStream methods mutate the borrowed `state` object in place
 * (the harness owns a private copy and echoes it as out.st, even on raise).
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   t                          agent.i18n.t(key)                 args{key}
 *   uuid4_hex                  uuid.uuid4().hex                  args{}
 *   cap_text                   _cap_text(text, keep)  [flow1]    args{text,keep}
 *   turn_start_index           api_server_turn_boundary.response_turn_start_index(ch,um,result)
 *                              args{conversation_history,user_message,result}
 *   message_response           cls._message_response(msg)        args{message}
 *   redact_api_error_text      _redact_api_error_text(value)     args{value}
 *   identify_output            api_server_response_identity.identify_output(items,response_id)
 *                              args{items,response_id}
 *   run_idempotency_scope      self._run_idempotency_scope(request)
 *                              args{request:{headers}}
 *   durable_response_id        api_server_response_identity.durable_response_id(key)
 *                              args{key}
 *   off_loop                   await store.off_loop(fn, *args)   args{fn,args}
 *                              fn markers: request_created_at, matching_replay
 *                              (the flow1-claimed _matching_replay runs as the
 *                              off_loop worker unit); store arg projects to
 *                              {"__store__":"response"}.
 *   time                       time.time()                       args{}
 *   active_authority           gateway.session_authorities.active_authority(runner)
 *                              args{gateway_runner}
 *   to_thread                  await asyncio.to_thread(implicit_api_session, persistence, sid, rid)
 *                              args{fn:"implicit_api_session",args:[persistence,sid,rid]}
 *   store_error_response       api_server_authority_runs.store_error_response(_openai_error, exc)
 *                              args{exc:{kind,msg}}
 *   error_response             _error_response(message, status, err_type=)
 *                              args{message,status,err_type}
 *   openai_error               _openai_error(message, err_type=, param=, code=)
 *                              args{message,err_type,param,code}
 *   logger.error               logger.error(fmt, *args, exc_info=True)
 *                              args{message,args,exc_info} (exc args -> str)
 *   make_request_fingerprint   _make_request_fingerprint(body, keys=)
 *                              args{body,keys}
 *   idem_get_or_set            await _idem_cache.get_or_set(scoped_key, fp, compute)
 *                              args{key_parts:[scope,route,key],fingerprint}
 *                              (the NUL-joined scoped_key projects to its
 *                              three parts — C strings cannot carry \0)
 *   compute                    await compute()                   args{}
 *   sse_headers                self._sse_headers(request)        args{request}
 *   stream_response            web.StreamResponse(status=, headers=)
 *                              args{status,headers} (ctor seam: the returned
 *                              object projects from the ARGS; the script only
 *                              gates a raise; ret projection
 *                              {"__response__":"stream",status,headers,prepared})
 *   response.prepare           await response.prepare(request)   args{request}
 *   response.write             await response.write(frame)       args{data}
 *   sse_frame                  _sse_frame(data, event=)          args{data,event}
 *   json_response              web.json_response(body, status=, headers=)
 *                              args{body,status,headers} (ctor seam; projection
 *                              {"__response__":"json",body,status,headers})
 *   current_response_store     self._current_response_store()    args{}
 *   request_binding            api_server_response_admissions.request_binding(self,request,durable_key)
 *                              args{request,durable_key}
 *   responses_fingerprint_keys _responses_fingerprint_keys(body) [flow1]
 *                              args{body}
 *   settled                    _settled(record)       [flow1]    args{record}
 *   store.get                  store.get(key)                    args{key}
 *   store.put                  store.put(key, value)             args{key,value}
 *   store.get_conversation     store.get_conversation(conv)      args{conversation}
 *   store.set_conversation     store.set_conversation(conv, rid) args{conversation,response_id}
 *   next_item_id               api_server_response_identity.next_item_id(rid,counts,kind,call_id=,commentary=)
 *                              args{response_id,identity_counts,kind,call_id,commentary}
 *   responses_usage_payload    self._api._responses_usage_payload(usage)
 *                              args{usage}
 *
 * Internal composition (REAL on both sides, no seam of its own):
 * _response_messages_turn_start_index inside _turn_transcript_messages /
 * _turn_reasoning_text; _message_item / _reasoning_item inside
 * _extract_output_items; envelope inside terminal_envelope;
 * _history_with_user inside _persist_snapshot; _persist_snapshot inside
 * _commit_terminal; _chat_request_id inside _authority_chat_kwargs /
 * _implicit_chat_session; _prepare_sse_response inside
 * _replay_sse_responses; _replay_sse_responses inside
 * _responses_durable_replay; json.loads / json.dumps inside
 * _trim_tool_items (Python-default formatting).
 *
 * Fixture domain (kept identical on both sides):
 * - request nodes carry ONLY {"headers":{...}}; gateway_runner is absent
 *   (AttributeError), null, or {"session_authority":...};
 * - durable_key is null or a 2-item array; authority scripts are null or
 *   objects with a "persistence" field; store markers are non-null objects;
 * - integral floats == ints (parsed JSON loses int/float identity); str()
 *   positions use integral ints or non-integral floats;
 * - uuid4_hex scripts are ASCII; reasoning/strip probes use ASCII whitespace;
 * - json.loads blobs use the common libjson/Python grammar (no NaN, leading
 *   zeros, raw control chars, invalid escapes); json.dumps numbers integral;
 * - scripted raises never use KeyError (str(KeyError) reprs its argument);
 *   live KeyError raises carry the repr'd key;
 * - events elements are 2-item arrays (string unpack-overflow messages are
 *   version-shaped and out of domain); tool_call roles are str/null;
 * - write_event data is an object; output_index an int; emitted item ids
 *   present on output_item.added frames.
 *
 * Self-contained: the oracle harness #includes this .c directly; only libc
 * and libjson are used.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */

static bool jtruth(const json_t *v) {
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
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static char *xsf(const char *fmt, ...) {
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

/* Python type name of a parsed JSON value (integral doubles read as int). */
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* Python == across JSON values (bool is an int subclass; deep containers). */
static bool py_eq(const json_t *a, const json_t *b) {
    bool ap = jpresent(a), bp = jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (is_bool(a) && is_bool(b)) return a->bool_val == b->bool_val;
    if ((is_bool(a) || is_num(a)) && (is_bool(b) || is_num(b))) {
        double av = is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (is_str(a) && is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (is_arr(a) && is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (is_obj(a) && is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
static bool str_eq(const json_t *v, const char *s) {
    return is_str(v) && v->str_val && !strcmp(v->str_val, s);
}

/* Shortest round-trip float repr (Python str(float)); integral -> int form. */
static char *py_float_str(double d) {
    if (d == (double)(long long)d) return xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* Python str() of a JSON value (containers: repr syntax; the fixture domain
 * keeps str()-coerced containers quote-free). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* ── UTF-8 char helpers (Python len/slice count CHARACTERS) ── */

static size_t utf8_advance(unsigned char c) {
    return (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
}
static size_t utf8_len(const char *s) {
    size_t n = 0;
    for (size_t i = 0; s[i]; ) { i += utf8_advance((unsigned char)s[i]); n++; }
    return n;
}
static size_t utf8_off(const char *s, size_t chars) {
    size_t i = 0, n = 0;
    while (s[i] && n < chars) { i += utf8_advance((unsigned char)s[i]); n++; }
    return i;
}

/* ASCII whitespace strip-nonempty probe (fixture domain: ASCII WS only;
 * Python str.strip() also strips \x1c-\x1f — included here). */
static bool ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || (c >= 0x1c && c <= 0x1f);
}
static bool strip_nonempty(const char *s) {
    if (!s) return false;
    size_t i = 0, n = strlen(s);
    while (i < n && ascii_ws((unsigned char)s[i])) i++;
    return i < n;
}

/* ── trace / seam / raise plumbing ── */

typedef struct or2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} or2_seams_t;

static json_t *or2_call(const or2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *or2_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
static bool or2_is_raise(const json_t *r) { return or2_raise_class(r) != NULL; }
static const char *or2_raise_msg(const json_t *r) {
    const json_t *m = is_obj(r) ? json_obj_get(r, "msg") : NULL;
    return (is_str(m) && m->str_val) ? m->str_val : "";
}

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *raise_type(const char *msg) { return raise_obj("TypeError", msg); }
static json_t *raise_attr(const json_t *v, const char *attr) {
    char *m = xsf("'%s' object has no attribute '%s'", py_type_name(v), attr);
    json_t *r = raise_obj("AttributeError", m);
    free(m);
    return r;
}
static json_t *raise_attr_ns(const char *attr) {
    char *m = xsf("'types.SimpleNamespace' object has no attribute '%s'", attr);
    json_t *r = raise_obj("AttributeError", m);
    free(m);
    return r;
}
static json_t *raise_key(const char *key) {
    char *m = xsf("'%s'", key);
    json_t *r = raise_obj("KeyError", m);
    free(m);
    return r;
}
/* Propagate a seam/helper raise result (transfers ownership). */
#define OR2_PROP(r) do { if (or2_is_raise(r)) return (r); } while (0)

/* ── exception hierarchy (FULL builtin tree + sqlite3 + persistence families;
 * first-base parent chain; unknown kinds default to the Exception family,
 * matching the oracle's synthetic Exception subclasses). ── */

typedef struct { const char *name, *parent; } exc_parent_t;
static const exc_parent_t EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
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
    {"WindowsError", "OSError"},
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
    {"JSONDecodeError", "ValueError"},
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
    /* sqlite3 family (Error/Warning are the sqlite3 classes) */
    {"Error", "Exception"},
    {"InterfaceError", "Error"},
    {"DatabaseError", "Error"},
    {"DataError", "DatabaseError"},
    {"OperationalError", "DatabaseError"},
    {"IntegrityError", "DatabaseError"},
    {"InternalError", "DatabaseError"},
    {"ProgrammingError", "DatabaseError"},
    {"NotSupportedError", "DatabaseError"},
    /* persistence taxonomy (persistence/core/errors.py) */
    {"PersistenceError", "Exception"},
    {"TransientStoreError", "PersistenceError"},
    {"PermanentStoreError", "PersistenceError"},
    {"StoreUnavailable", "PersistenceError"},
    {"StoreReplaced", "PersistenceError"},
    {"AuthorityRefusal", "PersistenceError"},
    {NULL, NULL}
};

static const char *exc_parent(const char *kind) {
    for (size_t i = 0; EXC_PARENTS[i].name; i++)
        if (!strcmp(EXC_PARENTS[i].name, kind)) return EXC_PARENTS[i].parent;
    return NULL;
}
/* except Exception catches the Exception family; the BaseException trio
 * (and CancelledError / exception groups) PASS THROUGH. Unknown kinds are
 * synthetic Exception subclasses (the oracle's default). */
static bool or2_exc_is_exception(const char *kind) {
    if (!kind) return true;
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "Exception")) return true;
        if (!strcmp(kind, "BaseException")) return false;
        const char *p = exc_parent(kind);
        if (!p) return true;  /* unknown -> synthetic Exception subclass */
        kind = p;
    }
    return true;
}
static bool or2_exc_is_base(const char *kind) { return !or2_exc_is_exception(kind); }
static bool or2_is_authority_refusal(const char *kind) {
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "AuthorityRefusal")) return true;
        const char *p = exc_parent(kind);
        if (!p) return false;
        kind = p;
    }
    return false;
}
/* except (AuthorityRefusal, sqlite3.Error, OSError, *STORE_FAILURES) */
static bool or2_is_store_catch(const char *kind) {
    for (int depth = 0; kind && depth < 64; depth++) {
        if (!strcmp(kind, "AuthorityRefusal") || !strcmp(kind, "TransientStoreError") ||
            !strcmp(kind, "PermanentStoreError") || !strcmp(kind, "StoreUnavailable") ||
            !strcmp(kind, "StoreReplaced") || !strcmp(kind, "Error") ||
            !strcmp(kind, "OSError"))
            return true;
        const char *p = exc_parent(kind);
        if (!p) return false;
        kind = p;
    }
    return false;
}

/* ── args builders (null-valued keys omitted on BOTH sides) ── */

static void aset_j(json_t *a, const char *k, const json_t *v) {
    if (jpresent(v)) json_set(a, k, json_copy(v));
}
static void aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── Python iteration / unpack / subscript semantics ── */

static json_t *py_iter_list(const json_t *v) {
    if (is_arr(v)) return json_copy(v);
    if (is_obj(v)) {
        json_t *out = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(out, json_string(v->c.keys[i]));
        return out;
    }
    if (is_str(v)) {
        json_t *out = json_array();
        const char *s = v->str_val ? v->str_val : "";
        for (size_t i = 0; s[i]; ) {
            size_t start = i;
            i += utf8_advance((unsigned char)s[i]);
            char *ch = xsf("%.*s", (int)(i - start), s + start);
            json_append(out, json_string(ch));
            free(ch);
        }
        return out;
    }
    char *m = xsf("'%s' object is not iterable", py_type_name(v));
    json_t *r = raise_type(m);
    free(m);
    return r;
}

/* a, b = v — NULL on success (a/b NEW copies), else a raise object.
 * Fixture domain: array iterables (string overflow messages differ). */
static json_t *unpack2(const json_t *v, json_t **a, json_t **b) {
    *a = *b = NULL;
    if (!is_arr(v)) {
        char *m = xsf("cannot unpack non-iterable %s object", py_type_name(v));
        json_t *r = raise_type(m);
        free(m);
        return r;
    }
    size_t n = v->c.count;
    if (n < 2) {
        char *m = xsf("not enough values to unpack (expected 2, got %zu)", n);
        return raise_obj("ValueError", m);
    }
    if (n > 2) {
        char *m = xsf("too many values to unpack (expected 2, got %zu)", n);
        return raise_obj("ValueError", m);
    }
    *a = json_copy(v->c.items[0]);
    *b = json_copy(v->c.items[1]);
    return NULL;
}

/* list[start:] for the transcript slicers. None is a LEGAL slice bound
 * (messages[None:] == the whole list); other non-integers raise. Negative
 * clamps like Python; non-list carriers raise. */
static json_t *slice_from(const json_t *arr, const json_t *start) {
    long long s;
    if (!start || start->type == JSON_NULL) s = 0;
    else if (is_bool(start)) s = start->bool_val ? 1 : 0;
    else if (is_num(start) && start->num_val == (double)(long long)start->num_val)
        s = (long long)start->num_val;
    else
        return raise_type("slice indices must be integers or None or have an __index__ method");
    if (!is_arr(arr)) {
        char *m = xsf("'%s' object is not subscriptable", py_type_name(arr));
        json_t *r = raise_type(m);
        free(m);
        return r;
    }
    long long n = (long long)arr->c.count;
    if (s < 0) { s += n; if (s < 0) s = 0; }
    if (s > n) s = n;
    json_t *out = json_array();
    for (long long i = s; i < n; i++) json_append(out, json_copy(arr->c.items[i]));
    return out;
}

/* obj.get(key, default) — present-null stays null; absent -> default copy. */
static json_t *get_def(const json_t *obj, const char *key, const char *def) {
    const json_t *v = is_obj(obj) ? json_obj_get(obj, key) : NULL;
    if (v) return json_copy(v);
    return def ? json_string(def) : json_null();
}

/* request.headers.get(key) — the request double always carries a headers
 * dict (the oracle materializes SimpleNamespace(headers={...})). */
static const json_t *header_get(const json_t *request, const char *key) {
    const json_t *h = is_obj(request) ? json_obj_get(request, "headers") : NULL;
    return is_obj(h) ? json_obj_get(h, key) : NULL;
}

/* getattr(self.gateway_runner, 'session_authority', None):
 * absent gateway_runner attr -> AttributeError; None runner -> None;
 * object runner -> the session_authority field (absent -> None default). */
static json_t *gw_session_authority(const json_t *gw) {
    if (!gw) return raise_attr_ns("gateway_runner");
    if (gw->type == JSON_NULL) return NULL;
    if (!is_obj(gw)) return raise_attr(gw, "session_authority");
    return json_obj_get(gw, "session_authority");  /* borrowed or NULL */
}

/* self.<attr> direct read on the SimpleNamespace state double. */
static json_t *sattr(const json_t *state, const char *key) {
    const json_t *v = is_obj(state) ? json_obj_get(state, key) : NULL;
    if (!v) return raise_attr_ns(key);
    return (json_t *)v;  /* borrowed */
}
/* ── Python json.loads / json.dumps (default formatting) ── */

/* json.loads(s): NULL on any parse failure (the live trimmer catches
 * ValueError). Fixture domain: the common libjson/Python grammar. */
static json_t *py_json_loads(const char *s) {
    char *err = NULL;
    json_t *v = json_parse(s, &err);
    free(err);
    return v;
}

typedef struct { char *buf; size_t len, cap; } sbuf_t;
static void sb_init(sbuf_t *b) { b->cap = 64; b->len = 0; b->buf = malloc(b->cap); b->buf[0] = '\0'; }
static void sb_putc(sbuf_t *b, char c) {
    if (b->len + 2 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void sb_puts(sbuf_t *b, const char *s) { while (*s) sb_putc(b, *s++); }
static void sb_json_string(sbuf_t *b, const char *s) {
    sb_putc(b, '"');
    for (size_t i = 0; s[i]; ) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') { sb_puts(b, "\\\""); i++; continue; }
        if (c == '\\') { sb_puts(b, "\\\\"); i++; continue; }
        if (c < 0x20) {
            switch (c) {
                case '\b': sb_puts(b, "\\b"); break;
                case '\f': sb_puts(b, "\\f"); break;
                case '\n': sb_puts(b, "\\n"); break;
                case '\r': sb_puts(b, "\\r"); break;
                case '\t': sb_puts(b, "\\t"); break;
                default: {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c);
                    sb_puts(b, tmp);
                }
            }
            i++;
            continue;
        }
        if (c < 0x7f) { sb_putc(b, (char)c); i++; continue; }
        /* non-ASCII: decode the codepoint, emit \uXXXX (ensure_ascii) */
        unsigned long cp;
        size_t adv = utf8_advance(c);
        if (adv == 2) cp = c & 0x1F;
        else if (adv == 3) cp = c & 0x0F;
        else cp = c & 0x07;
        for (size_t k = 1; k < adv && s[i + k]; k++)
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        i += adv;
        char tmp[16];
        if (cp >= 0x10000) {
            unsigned long v = cp - 0x10000;
            snprintf(tmp, sizeof(tmp), "\\u%04x\\u%04x",
                     (unsigned)(0xD800 + (v >> 10)), (unsigned)(0xDC00 + (v & 0x3FF)));
        } else {
            snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)cp);
        }
        sb_puts(b, tmp);
    }
    sb_putc(b, '"');
}
static void sb_json_value(sbuf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: sb_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char *s = py_float_str(v->num_val);
            sb_puts(b, s);
            free(s);
            return;
        }
        case JSON_STRING: sb_json_string(b, v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY: {
            sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                sb_json_value(b, v->c.items[i]);
            }
            sb_putc(b, ']');
            return;
        }
        case JSON_OBJECT: {
            sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                sb_json_string(b, v->c.keys[i]);
                sb_puts(b, ": ");
                sb_json_value(b, v->c.items[i]);
            }
            sb_putc(b, '}');
            return;
        }
    }
}
/* json.dumps(v), Python defaults: separators (', ', ': '), ensure_ascii.
 * Fixture domain: integral numbers only (float repr parity out of domain). */
static char *py_json_dumps(const json_t *v) {
    sbuf_t b;
    sb_init(&b);
    sb_json_value(&b, v);
    return b.buf;
}

/* uuid4().hex[:n] — the hex script is ASCII; a non-string hex raises the
 * subscript TypeError at the slice site. */
static json_t *uuid_hex_slice(const or2_seams_t *sp, size_t n, const char *prefix) {
    json_t *a = json_object();
    json_t *hex = or2_call(sp, "uuid4_hex", a);
    json_free(a);
    OR2_PROP(hex);
    if (!is_str(hex)) {
        char *m = xsf("'%s' object is not subscriptable", py_type_name(hex));
        json_t *r = raise_type(m);
        free(m);
        json_free(hex);
        return r;
    }
    const char *s = hex->str_val ? hex->str_val : "";
    size_t off = utf8_off(s, n);
    char *id = xsf("%s%.*s", prefix, (int)off, s);
    json_t *r = json_string(id);
    free(id);
    json_free(hex);
    return r;
}

/* ── module-level leaves ── */

/* PoP: or2_transformed_notice @ gateway/platforms/api_server_openai_routes.py:_transformed_notice */
json_t *or2_transformed_notice(const or2_seams_t *sp) {
    json_t *a = json_object();
    aset_s(a, "key", "platform.api_server.transformed_notice");
    json_t *r = or2_call(sp, "t", a);
    json_free(a);
    return r;
}

/* PoP: or2_message_item @ gateway/platforms/api_server_openai_routes.py:_message_item */
json_t *or2_message_item(const json_t *text) {
    json_t *part = json_object();
    json_set(part, "type", json_string("output_text"));
    json_set(part, "text", text ? json_copy(text) : json_null());
    json_t *content = json_array();
    json_append(content, part);
    json_t *out = json_object();
    json_set(out, "type", json_string("message"));
    json_set(out, "role", json_string("assistant"));
    json_set(out, "content", content);
    return out;
}

/* PoP: or2_reasoning_item @ gateway/platforms/api_server_openai_routes.py:_reasoning_item */
json_t *or2_reasoning_item(const json_t *text, const or2_seams_t *sp) {
    json_t *id = uuid_hex_slice(sp, 24, "rs_");
    OR2_PROP(id);
    json_t *part = json_object();
    json_set(part, "type", json_string("summary_text"));
    json_set(part, "text", text ? json_copy(text) : json_null());
    json_t *summary = json_array();
    json_append(summary, part);
    json_t *out = json_object();
    json_set(out, "id", id);
    json_set(out, "type", json_string("reasoning"));
    json_set(out, "status", json_string("completed"));
    json_set(out, "summary", summary);
    return out;
}

/* PoP: or2_trim_tool_items @ gateway/platforms/api_server_openai_routes.py:_trim_tool_items */
json_t *or2_trim_tool_items(const json_t *items, const or2_seams_t *sp) {
    json_t *it = py_iter_list(items);
    OR2_PROP(it);
    static const char *CAP_KEYS[] = {"content", "query", "pattern", "old_string", "new_string"};
    for (size_t i = 0; i < it->c.count; i++) {
        json_t *item = it->c.items[i];
        if (!is_obj(item)) {
            json_t *r = raise_attr(item, "get");
            json_free(it);
            return r;
        }
        const json_t *type = json_obj_get(item, "type");
        if (str_eq(type, "function_call")) {
            /* try: loads / cap / dumps; except Exception: pass */
            const json_t *raw = json_obj_get(item, "arguments");
            json_t *args = NULL;
            bool loads_failed = false;
            if (is_str(raw)) {
                args = py_json_loads(raw->str_val ? raw->str_val : "");
                if (!args) loads_failed = true;  /* ValueError -> except pass */
            } else {
                args = raw ? json_copy(raw) : json_object();
            }
            if (!loads_failed && is_obj(args)) {
                for (size_t k = 0; k < 5; k++) {
                    const json_t *v = json_obj_get(args, CAP_KEYS[k]);
                    if (is_str(v) && v->str_val && utf8_len(v->str_val) > 500) {
                        char *capped = xsf("[%zu chars \xe2\x80\x94 truncated for response.completed]",
                                           utf8_len(v->str_val));
                        json_set(args, CAP_KEYS[k], json_string(capped));
                        free(capped);
                    }
                }
                char *dumps = py_json_dumps(args);
                json_set(item, "arguments", json_string(dumps));
                free(dumps);
            }
            json_free(args);
        } else if (str_eq(type, "function_call_output")) {
            const json_t *output = json_obj_get(item, "output");
            json_t *empty = NULL;
            if (!output) { empty = json_array(); output = empty; }
            if (is_arr(output) && output->c.count > 0) {
                json_t *first = output->c.items[0];
                if (is_obj(first) && str_eq(json_obj_get(first, "type"), "input_text")) {
                    const json_t *text = json_obj_get(first, "text");
                    const char *ts = NULL;
                    if (!text) ts = "";
                    else if (is_str(text)) ts = text->str_val ? text->str_val : "";
                    else {
                        char *m = xsf("object of type '%s' has no len()", py_type_name(text));
                        json_t *r = raise_type(m);
                        free(m);
                        json_free(empty);
                        json_free(it);
                        return r;
                    }
                    if (utf8_len(ts) > 1000) {
                        json_t *a = json_object();
                        aset_s(a, "text", ts);
                        aset_n(a, "keep", 500);
                        json_t *capped = or2_call(sp, "cap_text", a);
                        json_free(a);
                        if (or2_is_raise(capped)) {
                            json_free(empty);
                            json_free(it);
                            return capped;
                        }
                        /* copy first BEFORE replacing output (json_set frees
                         * the old array that owns first). */
                        json_set(first, "text", capped);
                        json_t *newout = json_array();
                        json_append(newout, json_copy(first));
                        json_set(item, "output", newout);
                    }
                }
            }
            json_free(empty);
        }
    }
    return it;
}

/* ── turn-transcript cluster ── */

/* PoP: or2_turn_start_index @ gateway/platforms/api_server_openai_routes.py:_response_messages_turn_start_index */
json_t *or2_turn_start_index(const json_t *ch, const json_t *um, const json_t *result,
                             const or2_seams_t *sp) {
    json_t *a = json_object();
    aset_j(a, "conversation_history", ch);
    aset_j(a, "user_message", um);
    aset_j(a, "result", result);
    json_t *r = or2_call(sp, "turn_start_index", a);
    json_free(a);
    return r;
}

/* PoP: or2_turn_transcript_messages @ gateway/platforms/api_server_openai_routes.py:_turn_transcript_messages */
json_t *or2_turn_transcript_messages(const json_t *ch, const json_t *um, const json_t *result,
                                     const or2_seams_t *sp) {
    const json_t *messages = is_obj(result) ? json_obj_get(result, "messages") : NULL;
    if (!is_arr(messages) || messages->c.count == 0) return json_array();
    json_t *start = or2_turn_start_index(ch, um, result, sp);
    OR2_PROP(start);
    json_t *tail = slice_from(messages, start);
    json_free(start);
    OR2_PROP(tail);
    json_t *out = json_array();
    for (size_t i = 0; i < tail->c.count; i++) {
        const json_t *msg = tail->c.items[i];
        if (!is_obj(msg)) continue;
        const json_t *role = json_obj_get(msg, "role");
        if (!(str_eq(role, "assistant") || str_eq(role, "tool"))) continue;
        json_t *a = json_object();
        aset_j(a, "message", msg);
        json_t *proj = or2_call(sp, "message_response", a);
        json_free(a);
        if (or2_is_raise(proj)) { json_free(out); json_free(tail); return proj; }
        if (!is_obj(proj)) {
            json_t *r = raise_attr(proj, "get");
            json_free(proj); json_free(out); json_free(tail);
            return r;
        }
        if (!str_eq(json_obj_get(proj, "display_kind"), "hidden"))
            json_append(out, proj);
        else
            json_free(proj);
    }
    json_free(tail);
    return out;
}

/* PoP: or2_turn_reasoning_text @ gateway/platforms/api_server_openai_routes.py:_turn_reasoning_text */
json_t *or2_turn_reasoning_text(const json_t *ch, const json_t *um, const json_t *result,
                                const or2_seams_t *sp) {
    const json_t *messages = is_obj(result) ? json_obj_get(result, "messages") : NULL;
    if (!is_arr(messages)) return json_string("");
    json_t *start = or2_turn_start_index(ch, um, result, sp);
    OR2_PROP(start);
    json_t *tail = slice_from(messages, start);
    json_free(start);
    OR2_PROP(tail);
    char *joined = strdup("");
    size_t nparts = 0;
    for (size_t i = 0; i < tail->c.count; i++) {
        const json_t *msg = tail->c.items[i];
        if (!is_obj(msg)) continue;
        if (!str_eq(json_obj_get(msg, "role"), "assistant")) continue;
        const json_t *r = json_obj_get(msg, "reasoning");
        if (!is_str(r) || !strip_nonempty(r->str_val)) continue;
        char *next = xsf("%s%s%s", joined, nparts ? "\n\n" : "", r->str_val);
        free(joined);
        joined = next;
        nparts++;
    }
    json_t *out = json_string(joined);
    free(joined);
    json_free(tail);
    return out;
}

/* PoP: or2_extract_output_items @ gateway/platforms/api_server_openai_routes.py:_extract_output_items */
json_t *or2_extract_output_items(const json_t *result, const json_t *start_index,
                                 const json_t *response_id, const or2_seams_t *sp) {
    if (!is_obj(result)) return raise_attr(result, "get");
    const json_t *messages = json_obj_get(result, "messages");
    json_t *msgs = messages ? json_copy(messages) : json_array();
    /* if start_index > 0: messages = messages[start_index:] */
    bool gt = is_num(start_index) || is_bool(start_index);
    if (!gt) {
        char *m = xsf("'>' not supported between instances of '%s' and 'int'", py_type_name(start_index));
        json_t *r = raise_type(m);
        free(m);
        json_free(msgs);
        return r;
    }
    double si = is_bool(start_index) ? (start_index->bool_val ? 1.0 : 0.0) : start_index->num_val;
    if (si > 0) {
        if (is_arr(msgs)) {
            json_t *sl = slice_from(msgs, start_index);
            json_free(msgs);
            OR2_PROP(sl);
            msgs = sl;
        } else if (is_str(msgs)) {
            const char *s = msgs->str_val ? msgs->str_val : "";
            size_t off = utf8_off(s, (size_t)si);
            json_t *sl = json_string(s + off);
            json_free(msgs);
            msgs = sl;
        } else {
            char *m = xsf("'%s' object is not subscriptable", py_type_name(msgs));
            json_t *r = raise_type(m);
            free(m);
            json_free(msgs);
            return r;
        }
    }
    json_t *it = py_iter_list(msgs);
    json_free(msgs);
    OR2_PROP(it);
    json_t *items = json_array();
    for (size_t i = 0; i < it->c.count; i++) {
        const json_t *msg = it->c.items[i];
        if (!is_obj(msg)) {
            json_t *r = raise_attr(msg, "get");
            json_free(items); json_free(it);
            return r;
        }
        const json_t *role = json_obj_get(msg, "role");
        const json_t *reasoning = str_eq(role, "assistant") ? json_obj_get(msg, "reasoning") : NULL;
        if (is_str(reasoning) && strip_nonempty(reasoning->str_val)) {
            json_t *ri = or2_reasoning_item(reasoning, sp);
            if (or2_is_raise(ri)) { json_free(items); json_free(it); return ri; }
            json_append(items, ri);
        }
        const json_t *tcs = json_obj_get(msg, "tool_calls");
        if (str_eq(role, "assistant") && jtruth(tcs)) {
            json_t *tcit = py_iter_list(tcs);
            if (or2_is_raise(tcit)) { json_free(items); json_free(it); return tcit; }
            for (size_t j = 0; j < tcit->c.count; j++) {
                const json_t *tc = tcit->c.items[j];
                if (!is_obj(tc)) {
                    json_t *r = raise_attr(tc, "get");
                    json_free(tcit); json_free(items); json_free(it);
                    return r;
                }
                const json_t *func = json_obj_get(tc, "function");
                json_t *fempty = NULL;
                if (!func) { fempty = json_object(); func = fempty; }
                if (!is_obj(func)) {
                    json_t *r = raise_attr(func, "get");
                    json_free(fempty); json_free(tcit); json_free(items); json_free(it);
                    return r;
                }
                json_t *id = uuid_hex_slice(sp, 24, "fc_");
                if (or2_is_raise(id)) {
                    json_free(fempty); json_free(tcit); json_free(items); json_free(it);
                    return id;
                }
                json_t *item = json_object();
                json_set(item, "id", id);
                json_set(item, "type", json_string("function_call"));
                json_set(item, "status", json_string("completed"));
                json_set(item, "name", get_def(func, "name", ""));
                json_set(item, "arguments", get_def(func, "arguments", ""));
                json_set(item, "call_id", get_def(tc, "id", ""));
                json_append(items, item);
                json_free(fempty);
            }
            json_free(tcit);
        } else if (str_eq(role, "tool")) {
            json_t *id = uuid_hex_slice(sp, 24, "fco_");
            if (or2_is_raise(id)) { json_free(items); json_free(it); return id; }
            json_t *item = json_object();
            json_set(item, "id", id);
            json_set(item, "type", json_string("function_call_output"));
            json_set(item, "status", json_string("completed"));
            json_set(item, "call_id", get_def(msg, "tool_call_id", ""));
            json_set(item, "output", get_def(msg, "content", ""));
            json_append(items, item);
        }
    }
    json_free(it);
    /* final = result.get("final_response", "") or redact(result.get("error", default)) */
    json_t *final = get_def(result, "final_response", "");
    if (!jtruth(final)) {
        json_free(final);
        json_t *err = get_def(result, "error", "(No response generated)");
        json_t *a = json_object();
        aset_j(a, "value", err);
        json_free(err);
        final = or2_call(sp, "redact_api_error_text", a);
        json_free(a);
        if (or2_is_raise(final)) { json_free(items); return final; }
    }
    json_t *mi = or2_message_item(final);
    json_free(final);
    json_append(items, mi);
    if (jpresent(response_id)) {
        json_t *a = json_object();
        aset_j(a, "items", items);
        aset_j(a, "response_id", response_id);
        json_t *r = or2_call(sp, "identify_output", a);
        json_free(a);
        json_free(items);
        return r;
    }
    return items;
}
/* ── chat admission identity ── */

/* PoP: or2_chat_request_id @ gateway/platforms/api_server_openai_routes.py:_chat_request_id */
json_t *or2_chat_request_id(const json_t *request, const or2_seams_t *sp) {
    const json_t *key = header_get(request, "Idempotency-Key");
    if (!jtruth(key)) return NULL;
    json_t *a = json_object();
    aset_j(a, "request", request);
    json_t *scope = or2_call(sp, "run_idempotency_scope", a);
    json_free(a);
    OR2_PROP(scope);
    char *ss = py_str(scope);
    char *ks = py_str(key);
    char *id = xsf("chat:%s:%s", ss, ks);
    free(ss); free(ks);
    json_free(scope);
    json_t *r = json_string(id);
    free(id);
    return r;
}

/* PoP: or2_authority_chat_kwargs @ gateway/platforms/api_server_openai_routes.py:_authority_chat_kwargs */
json_t *or2_authority_chat_kwargs(const json_t *gw, const json_t *request,
                                  const json_t *provided_session_id, const or2_seams_t *sp) {
    json_t *sa = gw_session_authority(gw);
    OR2_PROP(sa);
    if (!jpresent(sa)) return json_object();
    /* dict kwargs evaluate left-to-right: request_id (composed sibling,
     * fires the scope seam when a key is present), then the scope seam again. */
    json_t *rid = or2_chat_request_id(request, sp);
    OR2_PROP(rid);
    json_t *a = json_object();
    aset_j(a, "request", request);
    json_t *scope = or2_call(sp, "run_idempotency_scope", a);
    json_free(a);
    if (or2_is_raise(scope)) { json_free(rid); return scope; }
    json_t *out = json_object();
    json_set(out, "request_id", rid ? rid : json_null());
    json_set(out, "history_from_session", json_bool(jtruth(provided_session_id)));
    json_set(out, "run_owner_scope", scope);
    return out;
}

/* ── async admission flows ── */

/* PoP: or2_response_identity @ gateway/platforms/api_server_openai_routes.py:_response_identity */
json_t *or2_response_identity(const json_t *durable_key, const or2_seams_t *sp) {
    if (jpresent(durable_key)) {
        if (!is_arr(durable_key) || durable_key->c.count == 0) {
            char *m = xsf("'%s' object is not subscriptable", py_type_name(durable_key));
            json_t *r = raise_type(m);
            free(m);
            return r;
        }
        const json_t *key0 = durable_key->c.items[0];
        json_t *a = json_object();
        aset_j(a, "key", key0);
        json_t *drid = or2_call(sp, "durable_response_id", a);
        json_free(a);
        OR2_PROP(drid);
        json_t *olargs = json_array();
        json_append(olargs, key0 ? json_copy(key0) : json_null());
        a = json_object();
        aset_s(a, "fn", "request_created_at");
        json_set(a, "args", olargs);
        json_t *created = or2_call(sp, "off_loop", a);
        json_free(a);
        if (or2_is_raise(created)) { json_free(drid); return created; }
        json_t *out = json_array();
        json_append(out, drid);
        json_append(out, created);
        return out;
    }
    json_t *id = uuid_hex_slice(sp, 28, "resp_");
    OR2_PROP(id);
    json_t *a = json_object();
    json_t *t = or2_call(sp, "time", a);
    json_free(a);
    if (or2_is_raise(t)) { json_free(id); return t; }
    if (!is_num(t) && !is_bool(t)) {
        char *m = xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                      py_type_name(t));
        json_t *r = raise_type(m);
        free(m);
        json_free(t); json_free(id);
        return r;
    }
    double tv = is_bool(t) ? (t->bool_val ? 1.0 : 0.0) : t->num_val;
    json_free(t);
    json_t *out = json_array();
    json_append(out, id);
    json_append(out, json_number((double)(long long)tv));
    return out;
}

/* _implicit_chat_session TRY REGION only: authority.persistence read,
 * _chat_request_id (argument evaluation) and the to_thread worker seam.
 * The getattr gate and active_authority call sit OUTSIDE the live try. */
static json_t *implicit_body(const json_t *auth, const json_t *request, const json_t *session_id,
                             const or2_seams_t *sp) {
    /* authority.persistence (attribute double read) */
    const json_t *pers = NULL;
    if (is_obj(auth)) {
        pers = json_obj_get(auth, "persistence");
        if (!pers) return raise_attr_ns("persistence");
    } else {
        return raise_attr(auth, "persistence");
    }
    /* _chat_request_id is an ARGUMENT to to_thread: its seam fires first. */
    json_t *rid = or2_chat_request_id(request, sp);
    OR2_PROP(rid);
    json_t *olargs = json_array();
    json_append(olargs, json_copy(pers));
    json_append(olargs, session_id ? json_copy(session_id) : json_null());
    json_append(olargs, rid ? rid : json_null());
    json_t *a = json_object();
    aset_s(a, "fn", "implicit_api_session");
    json_set(a, "args", olargs);
    json_t *r = or2_call(sp, "to_thread", a);
    json_free(a);
    OR2_PROP(r);
    json_t *out = json_array();
    json_append(out, r);
    json_append(out, json_null());
    return out;
}

/* PoP: or2_implicit_chat_session @ gateway/platforms/api_server_openai_routes.py:_implicit_chat_session */
json_t *or2_implicit_chat_session(const json_t *gw, const json_t *request, const json_t *session_id,
                                  const or2_seams_t *sp) {
    /* Outside the try: the authority gate + active_authority probe. */
    json_t *sa = gw_session_authority(gw);
    OR2_PROP(sa);
    if (!jpresent(sa)) {
        json_t *out = json_array();
        json_append(out, session_id ? json_copy(session_id) : json_null());
        json_append(out, json_null());
        return out;
    }
    json_t *a = json_object();
    aset_j(a, "gateway_runner", gw);
    json_t *auth = or2_call(sp, "active_authority", a);
    json_free(a);
    OR2_PROP(auth);
    if (!auth) {
        json_t *out = json_array();
        json_append(out, session_id ? json_copy(session_id) : json_null());
        json_append(out, json_null());
        return out;
    }
    json_t *r = implicit_body(auth, request, session_id, sp);
    json_free(auth);
    if (!or2_is_raise(r)) return r;
    const char *kind = or2_raise_class(r);
    if (or2_exc_is_base(kind)) return r;
    const char *msg = or2_raise_msg(r);
    json_t *out = json_array();
    json_append(out, session_id ? json_copy(session_id) : json_null());
    if (or2_is_store_catch(kind)) {
        json_t *exc = json_object();
        json_set(exc, "kind", json_string(kind));
        json_set(exc, "msg", json_string(msg));
        json_t *a = json_object();
        json_set(a, "exc", exc);
        json_t *ser = or2_call(sp, "store_error_response", a);
        json_free(a);
        json_free(r);
        if (or2_is_raise(ser)) { json_free(out); return ser; }
        json_append(out, ser);
        return out;
    }
    /* except Exception: logger.error + _error_response(500) */
    json_t *la = json_object();
    aset_s(la, "message", "Error resolving the chat completions session: %s");
    json_t *largs = json_array();
    json_append(largs, json_string(msg));
    json_set(la, "args", largs);
    aset_b(la, "exc_info", true);
    json_t *lr = or2_call(sp, "logger.error", la);
    json_free(la);
    json_free(r);
    OR2_PROP(lr);
    json_free(lr);
    char *m = xsf("Internal server error: %s", msg);
    a = json_object();
    aset_s(a, "message", m);
    aset_n(a, "status", 500);
    aset_s(a, "err_type", "server_error");
    free(m);
    json_t *er = or2_call(sp, "error_response", a);
    json_free(a);
    if (or2_is_raise(er)) { json_free(out); return er; }
    json_append(out, er);
    return out;
}

/* _run_idempotent try body. */
static json_t *idempotent_body(const json_t *gw, const json_t *request, const json_t *body,
                               const json_t *fingerprint_keys, const json_t *route,
                               const or2_seams_t *sp) {
    const json_t *key = header_get(request, "Idempotency-Key");
    json_t *sa = gw_session_authority(gw);
    OR2_PROP(sa);
    json_t *r = NULL;
    if (jtruth(key) && !jpresent(sa)) {
        json_t *a = json_object();
        aset_j(a, "request", request);
        json_t *scope = or2_call(sp, "run_idempotency_scope", a);
        json_free(a);
        OR2_PROP(scope);
        a = json_object();
        aset_j(a, "body", body);
        aset_j(a, "keys", fingerprint_keys);
        json_t *fp = or2_call(sp, "make_request_fingerprint", a);
        json_free(a);
        if (or2_is_raise(fp)) { json_free(scope); return fp; }
        /* scoped_key = f"{scope}\0{route}\0{key}" -> parts projection */
        char *ss = py_str(scope), *rs = py_str(route), *ks = py_str(key);
        json_free(scope);
        json_t *parts = json_array();
        json_append(parts, json_string(ss));
        json_append(parts, json_string(rs));
        json_append(parts, json_string(ks));
        free(ss); free(rs); free(ks);
        a = json_object();
        json_set(a, "key_parts", parts);
        aset_j(a, "fingerprint", fp);
        json_free(fp);
        r = or2_call(sp, "idem_get_or_set", a);
        json_free(a);
        OR2_PROP(r);
    } else {
        json_t *a = json_object();
        r = or2_call(sp, "compute", a);
        json_free(a);
        OR2_PROP(r);
    }
    json_t *result = NULL, *usage = NULL;
    json_t *ur = unpack2(r, &result, &usage);
    json_free(r);
    OR2_PROP(ur);
    json_t *inner = json_array();
    json_append(inner, result);
    json_append(inner, usage);
    json_t *out = json_array();
    json_append(out, inner);
    json_append(out, json_null());
    return out;
}

/* PoP: or2_run_idempotent @ gateway/platforms/api_server_openai_routes.py:_run_idempotent */
json_t *or2_run_idempotent(const json_t *gw, const json_t *request, const json_t *body,
                           const json_t *log_label, const json_t *fingerprint_keys,
                           const json_t *route, const or2_seams_t *sp) {
    json_t *r = idempotent_body(gw, request, body, fingerprint_keys, route, sp);
    if (!or2_is_raise(r)) return r;
    const char *kind = or2_raise_class(r);
    if (or2_exc_is_base(kind)) return r;
    const char *msg = or2_raise_msg(r);
    if (or2_is_authority_refusal(kind)) {
        json_t *exc = json_object();
        json_set(exc, "kind", json_string(kind));
        json_set(exc, "msg", json_string(msg));
        json_t *a = json_object();
        json_set(a, "exc", exc);
        json_t *ser = or2_call(sp, "store_error_response", a);
        json_free(a);
        json_free(r);
        OR2_PROP(ser);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, ser);
        return out;
    }
    /* except Exception: logger.error(fmt, log_label, e, exc_info=True) */
    json_t *la = json_object();
    aset_s(la, "message", "Error running agent for %s: %s");
    json_t *largs = json_array();
    json_append(largs, log_label ? json_copy(log_label) : json_null());
    json_append(largs, json_string(msg));
    json_set(la, "args", largs);
    aset_b(la, "exc_info", true);
    json_t *lr = or2_call(sp, "logger.error", la);
    json_free(la);
    if (or2_is_raise(lr)) { json_free(r); return lr; }
    json_free(lr);
    /* getattr(e, "_notification_presentation_suppressed", False) is True */
    const json_t *ps = json_obj_get(r, "_notification_presentation_suppressed");
    bool muted = is_bool(ps) && ps->bool_val;
    json_free(r);
    char *m = muted ? strdup("") : xsf("Internal server error: %s", msg);
    json_t *a = json_object();
    aset_s(a, "message", m);
    aset_n(a, "status", 500);
    aset_s(a, "err_type", "server_error");
    free(m);
    json_t *er = or2_call(sp, "error_response", a);
    json_free(a);
    OR2_PROP(er);
    json_t *out = json_array();
    json_append(out, json_null());
    json_append(out, er);
    return out;
}

/* ── SSE response constructors / replay ── */

/* web.json_response ctor seam: the script gates a raise; the returned
 * object projects from the ARGS (identical on both sides). */
static json_t *json_response_ctor(const json_t *body, double status, const json_t *headers,
                                  const or2_seams_t *sp) {
    json_t *a = json_object();
    aset_j(a, "body", body);
    aset_n(a, "status", status);
    aset_j(a, "headers", headers);
    json_t *r = or2_call(sp, "json_response", a);
    json_free(a);
    OR2_PROP(r);
    json_free(r);
    json_t *proj = json_object();
    json_set(proj, "__response__", json_string("json"));
    json_set(proj, "body", body ? json_copy(body) : json_null());
    json_set(proj, "status", json_number(status));
    json_set(proj, "headers", is_obj(headers) ? json_copy(headers) : json_object());
    return proj;
}

/* PoP: or2_prepare_sse_response @ gateway/platforms/api_server_openai_routes.py:_prepare_sse_response */
json_t *or2_prepare_sse_response(const json_t *request, const json_t *session_id,
                                 const json_t *gateway_session_key, const or2_seams_t *sp) {
    json_t *a = json_object();
    aset_j(a, "request", request);
    json_t *headers = or2_call(sp, "sse_headers", a);
    json_free(a);
    OR2_PROP(headers);
    if (jtruth(session_id)) {
        if (!is_obj(headers)) {
            char *m = xsf("'%s' object does not support item assignment", py_type_name(headers));
            json_t *r = raise_type(m);
            free(m);
            json_free(headers);
            return r;
        }
        json_set(headers, "X-Hermes-Session-Id", json_copy(session_id));
    }
    if (jtruth(gateway_session_key)) {
        if (!is_obj(headers)) {
            char *m = xsf("'%s' object does not support item assignment", py_type_name(headers));
            json_t *r = raise_type(m);
            free(m);
            json_free(headers);
            return r;
        }
        json_set(headers, "X-Hermes-Session-Key", json_copy(gateway_session_key));
    }
    a = json_object();
    aset_n(a, "status", 200);
    aset_j(a, "headers", headers);
    json_t *sr = or2_call(sp, "stream_response", a);
    json_free(a);
    if (or2_is_raise(sr)) { json_free(headers); return sr; }
    json_free(sr);
    json_t *resp = json_object();
    json_set(resp, "__response__", json_string("stream"));
    json_set(resp, "status", json_number(200));
    json_set(resp, "headers", is_obj(headers) ? json_copy(headers) : json_object());
    json_set(resp, "prepared", json_bool(false));
    json_free(headers);
    a = json_object();
    aset_j(a, "request", request);
    json_t *pr = or2_call(sp, "response.prepare", a);
    json_free(a);
    if (or2_is_raise(pr)) { json_free(resp); return pr; }
    json_free(pr);
    json_set(resp, "prepared", json_bool(true));
    return resp;
}

/* PoP: or2_replay_sse_responses @ gateway/platforms/api_server_openai_routes.py:_replay_sse_responses */
json_t *or2_replay_sse_responses(const json_t *request, const json_t *replay, const or2_seams_t *sp) {
    if (!is_obj(replay)) return raise_attr(replay, "get");
    const json_t *headers = json_obj_get(replay, "headers");
    json_t *h = jtruth(headers) ? json_copy(headers) : json_object();
    const json_t *sid = json_obj_get(h, "X-Hermes-Session-Id");
    const json_t *gsk = json_obj_get(h, "X-Hermes-Session-Key");
    json_t *resp = or2_prepare_sse_response(request, sid, gsk, sp);
    if (or2_is_raise(resp)) { json_free(h); return resp; }
    const json_t *events = json_obj_get(replay, "events");
    json_t *evlist = NULL;
    if (!jtruth(events)) {
        const json_t *env = json_obj_get(replay, "response");
        if (!env) { json_free(h); json_free(resp); return raise_key("response"); }
        if (!is_obj(env)) {
            json_t *r;
            if (is_arr(env)) r = raise_type("list indices must be integers, not str");
            else {
                char *m = xsf("'%s' object is not subscriptable", py_type_name(env));
                r = raise_type(m);
                free(m);
            }
            json_free(h); json_free(resp);
            return r;
        }
        static const char *CKEYS[] = {"id", "object", "created_at", "model"};
        json_t *created = json_object();
        for (size_t i = 0; i < 4; i++) {
            const json_t *v = json_obj_get(env, CKEYS[i]);
            if (!v) {
                json_t *r = raise_key(CKEYS[i]);
                json_free(created); json_free(h); json_free(resp);
                return r;
            }
            json_set(created, CKEYS[i], json_copy(v));
        }
        json_set(created, "status", json_string("in_progress"));
        json_set(created, "output", json_array());
        const json_t *status = json_obj_get(env, "status");
        if (!status) {
            json_t *r = raise_key("status");
            json_free(created); json_free(h); json_free(resp);
            return r;
        }
        char *tname = xsf("response.%s", status->str_val ? status->str_val : "");
        evlist = json_array();
        json_t *e1 = json_array();
        json_append(e1, json_string("response.created"));
        json_t *d1 = json_object();
        json_set(d1, "type", json_string("response.created"));
        json_set(d1, "response", created);
        json_set(d1, "sequence_number", json_number(0));
        json_append(e1, d1);
        json_append(evlist, e1);
        json_t *e2 = json_array();
        json_append(e2, json_string(tname));
        json_t *d2 = json_object();
        json_set(d2, "type", json_string(tname));
        json_set(d2, "response", json_copy(env));
        json_set(d2, "sequence_number", json_number(1));
        json_append(e2, d2);
        json_append(evlist, e2);
        free(tname);
    } else {
        evlist = py_iter_list(events);
        if (or2_is_raise(evlist)) { json_free(h); json_free(resp); return evlist; }
    }
    json_free(h);
    for (size_t i = 0; i < evlist->c.count; i++) {
        json_t *etype = NULL, *data = NULL;
        json_t *ur = unpack2(evlist->c.items[i], &etype, &data);
        if (ur) { json_free(evlist); json_free(resp); return ur; }
        json_t *a = json_object();
        aset_j(a, "data", data);
        aset_j(a, "event", etype);
        json_t *frame = or2_call(sp, "sse_frame", a);
        json_free(a);
        if (or2_is_raise(frame)) {
            json_free(etype); json_free(data); json_free(evlist); json_free(resp);
            return frame;
        }
        a = json_object();
        aset_j(a, "data", frame);
        json_t *w = or2_call(sp, "response.write", a);
        json_free(a);
        json_free(frame);
        json_free(etype); json_free(data);
        if (or2_is_raise(w)) { json_free(evlist); json_free(resp); return w; }
        json_free(w);
    }
    json_free(evlist);
    return resp;
}

/* _openai_error('admission_conflict', code='admission_conflict') seam. */
static json_t *conflict_error(const or2_seams_t *sp) {
    json_t *a = json_object();
    aset_s(a, "message", "admission_conflict");
    aset_s(a, "err_type", "invalid_request_error");
    aset_s(a, "code", "admission_conflict");
    json_t *r = or2_call(sp, "openai_error", a);
    json_free(a);
    return r;
}

static json_t *arr4(json_t *a, json_t *b, json_t *c, json_t *d) {
    json_t *out = json_array();
    json_append(out, a ? a : json_null());
    json_append(out, b ? b : json_null());
    json_append(out, c ? c : json_null());
    json_append(out, d ? d : json_null());
    return out;
}

/* PoP: or2_responses_durable_replay @ gateway/platforms/api_server_openai_routes.py:_responses_durable_replay */
json_t *or2_responses_durable_replay(const json_t *gw, const json_t *request, const json_t *body,
                                     const json_t *stream, const or2_seams_t *sp) {
    const json_t *key = header_get(request, "Idempotency-Key");
    json_t *sa = gw_session_authority(gw);
    OR2_PROP(sa);
    bool canonical = jpresent(sa);
    json_t *dk = NULL, *scope = NULL;
    if (canonical && jtruth(key)) {
        json_t *a = json_object();
        aset_j(a, "request", request);
        scope = or2_call(sp, "run_idempotency_scope", a);
        json_free(a);
        OR2_PROP(scope);
        a = json_object();
        aset_j(a, "body", body);
        json_t *fk = or2_call(sp, "responses_fingerprint_keys", a);
        json_free(a);
        if (or2_is_raise(fk)) { json_free(scope); return fk; }
        a = json_object();
        aset_j(a, "body", body);
        aset_j(a, "keys", fk);
        json_free(fk);
        json_t *fp = or2_call(sp, "make_request_fingerprint", a);
        json_free(a);
        if (or2_is_raise(fp)) { json_free(scope); return fp; }
        char *ss = py_str(scope), *ks = py_str(key);
        char *kstr = xsf("idem:%s:%s", ss, ks);
        free(ss); free(ks);
        dk = json_array();
        json_append(dk, json_string(kstr));
        free(kstr);
        json_append(dk, fp);
        /* response_store = self._current_response_store() */
        a = json_object();
        json_t *store = or2_call(sp, "current_response_store", a);
        json_free(a);
        if (or2_is_raise(store)) { json_free(dk); json_free(scope); return store; }
        /* binding = request_binding(...) fires BEFORE the off_loop attribute
         * access on a maybe-None store (live statement order). */
        a = json_object();
        aset_j(a, "request", request);
        aset_j(a, "durable_key", dk);
        json_t *binding = or2_call(sp, "request_binding", a);
        json_free(a);
        if (or2_is_raise(binding)) { json_free(store); json_free(dk); json_free(scope); return binding; }
        if (!store) {
            json_free(binding); json_free(dk); json_free(scope);
            return raise_attr(NULL, "off_loop");
        }
        json_free(store);
        /* off_loop(_matching_replay, store, dk[0], binding) — the flow1
         * sibling runs as the worker unit; the store projects to a marker. */
        json_t *olargs = json_array();
        json_t *marker = json_object();
        json_set(marker, "__store__", json_string("response"));
        json_append(olargs, marker);
        json_append(olargs, json_copy(dk->c.items[0]));
        json_append(olargs, binding);
        a = json_object();
        aset_s(a, "fn", "matching_replay");
        json_set(a, "args", olargs);
        json_t *mr = or2_call(sp, "off_loop", a);
        json_free(a);
        if (or2_is_raise(mr)) { json_free(dk); json_free(scope); return mr; }
        json_t *matches = NULL, *replay = NULL;
        json_t *ur = unpack2(mr, &matches, &replay);
        json_free(mr);
        if (ur) { json_free(dk); json_free(scope); return ur; }
        if (!jtruth(matches)) {
            json_free(matches); json_free(replay);
            json_t *oe = conflict_error(sp);
            if (or2_is_raise(oe)) { json_free(dk); json_free(scope); return oe; }
            json_t *jr = json_response_ctor(oe, 409, NULL, sp);
            json_free(oe);
            if (or2_is_raise(jr)) { json_free(dk); json_free(scope); return jr; }
            return arr4(dk, scope, key ? json_copy(key) : NULL, jr);
        }
        json_free(matches);
        if (jpresent(replay)) {
            if (!is_obj(replay)) {
                json_t *r = raise_attr(replay, "get");
                json_free(replay); json_free(dk); json_free(scope);
                return r;
            }
            const json_t *rfp = json_obj_get(replay, "fingerprint");
            if (!py_eq(rfp, dk->c.items[1])) {
                json_free(replay);
                json_t *oe = conflict_error(sp);
                if (or2_is_raise(oe)) { json_free(dk); json_free(scope); return oe; }
                json_t *jr = json_response_ctor(oe, 409, NULL, sp);
                json_free(oe);
                if (or2_is_raise(jr)) { json_free(dk); json_free(scope); return jr; }
                return arr4(dk, scope, key ? json_copy(key) : NULL, jr);
            }
            a = json_object();
            aset_j(a, "record", replay);
            json_t *st = or2_call(sp, "settled", a);
            json_free(a);
            if (or2_is_raise(st)) { json_free(replay); json_free(dk); json_free(scope); return st; }
            bool settled = jtruth(st);
            json_free(st);
            if (!settled) {
                json_free(replay);
                a = json_object();
                aset_s(a, "message", "unknown_execution");
                aset_s(a, "err_type", "invalid_request_error");
                aset_s(a, "code", "unknown_execution");
                json_t *oe = or2_call(sp, "openai_error", a);
                json_free(a);
                if (or2_is_raise(oe)) { json_free(dk); json_free(scope); return oe; }
                json_t *jr = json_response_ctor(oe, 409, NULL, sp);
                json_free(oe);
                if (or2_is_raise(jr)) { json_free(dk); json_free(scope); return jr; }
                return arr4(dk, scope, key ? json_copy(key) : NULL, jr);
            }
            if (jtruth(stream)) {
                json_t *rs = or2_replay_sse_responses(request, replay, sp);
                json_free(replay);
                if (or2_is_raise(rs)) { json_free(dk); json_free(scope); return rs; }
                return arr4(dk, scope, key ? json_copy(key) : NULL, rs);
            }
            const json_t *renv = json_obj_get(replay, "response");
            if (!renv) {
                json_free(replay); json_free(dk); json_free(scope);
                return raise_key("response");
            }
            const json_t *rh = json_obj_get(replay, "headers");
            json_t *empty = NULL;
            if (!jtruth(rh)) { empty = json_object(); rh = empty; }
            json_t *jr = json_response_ctor(renv, 200, rh, sp);
            json_free(empty);
            json_free(replay);
            if (or2_is_raise(jr)) { json_free(dk); json_free(scope); return jr; }
            return arr4(dk, scope, key ? json_copy(key) : NULL, jr);
        }
        json_free(replay);
    }
    return arr4(dk, scope, key ? json_copy(key) : NULL, NULL);
}
/* ── _ResponsesStream state/method cores ──
 * `state` is the SimpleNamespace attribute window (borrowed; the harness
 * owns a private copy). Direct self.<attr> reads raise the prefixed
 * SimpleNamespace AttributeError when absent. Injected collaborators
 * (response_store / _api / response) are always present and enter as seams. */

/* PoP: or2_rs_item_id @ gateway/platforms/api_server_openai_routes.py:_item_id */
json_t *or2_rs_item_id(json_t *state, const json_t *kind, const json_t *call_id,
                       bool commentary, const or2_seams_t *sp) {
    json_t *rid = sattr(state, "response_id");
    OR2_PROP(rid);
    json_t *counts = sattr(state, "_identity_counts");
    OR2_PROP(counts);
    json_t *a = json_object();
    aset_j(a, "response_id", rid);
    aset_j(a, "identity_counts", counts);
    aset_j(a, "kind", kind);
    aset_j(a, "call_id", call_id);
    aset_b(a, "commentary", commentary);
    json_t *r = or2_call(sp, "next_item_id", a);
    json_free(a);
    return r;
}

/* PoP: or2_rs_envelope @ gateway/platforms/api_server_openai_routes.py:envelope */
json_t *or2_rs_envelope(json_t *state, const json_t *status) {
    json_t *rid = sattr(state, "response_id");
    OR2_PROP(rid);
    json_t *ca = sattr(state, "created_at");
    OR2_PROP(ca);
    json_t *model = sattr(state, "model");
    OR2_PROP(model);
    json_t *out = json_object();
    json_set(out, "id", json_copy(rid));
    json_set(out, "object", json_string("response"));
    json_set(out, "status", status ? json_copy(status) : json_null());
    json_set(out, "created_at", json_copy(ca));
    json_set(out, "model", json_copy(model));
    return out;
}

/* PoP: or2_rs_terminal_envelope @ gateway/platforms/api_server_openai_routes.py:terminal_envelope */
json_t *or2_rs_terminal_envelope(json_t *state, const json_t *status, const json_t *output,
                                 const json_t *error, const or2_seams_t *sp) {
    json_t *env = or2_rs_envelope(state, status);
    OR2_PROP(env);
    json_set(env, "output", output ? json_copy(output) : json_null());
    if (jpresent(error)) {
        json_t *dk = sattr(state, "durable_key");
        if (or2_is_raise(dk)) { json_free(env); return dk; }
        json_t *eobj = json_object();
        if (jtruth(dk)) {
            json_set(eobj, "code", json_string("agent_error"));
            json_set(eobj, "message", json_copy(error));
        } else {
            json_set(eobj, "message", json_copy(error));
            json_set(eobj, "type", json_string("server_error"));
        }
        json_set(env, "error", eobj);
    }
    json_t *usage = sattr(state, "usage");
    if (or2_is_raise(usage)) { json_free(env); return usage; }
    json_t *a = json_object();
    aset_j(a, "usage", usage);
    json_t *up = or2_call(sp, "responses_usage_payload", a);
    json_free(a);
    if (or2_is_raise(up)) { json_free(env); return up; }
    json_set(env, "usage", up);
    return env;
}

/* PoP: or2_rs_history_with_user @ gateway/platforms/api_server_openai_routes.py:_history_with_user */
json_t *or2_rs_history_with_user(json_t *state) {
    json_t *ch = sattr(state, "conversation_history");
    OR2_PROP(ch);
    json_t *um = sattr(state, "user_message");
    OR2_PROP(um);
    json_t *out = py_iter_list(ch);
    OR2_PROP(out);
    json_t *row = json_object();
    json_set(row, "role", json_string("user"));
    json_set(row, "content", um ? json_copy(um) : json_null());
    json_append(out, row);
    return out;
}

/* durable_key[0] for the store-unit cores (domain: null or 2-item array). */
static json_t *dk_first(const json_t *dk) {
    if (is_arr(dk) && dk->c.count > 0) return json_copy(dk->c.items[0]);
    char *m = xsf("'%s' object is not subscriptable", py_type_name(dk));
    json_t *r = raise_type(m);
    free(m);
    return r;
}

/* PoP: or2_rs_persist_snapshot @ gateway/platforms/api_server_openai_routes.py:_persist_snapshot */
json_t *or2_rs_persist_snapshot(json_t *state, const json_t *response_env, const json_t *history,
                                const json_t *session_id, const or2_seams_t *sp) {
    json_t *dk = sattr(state, "durable_key");
    OR2_PROP(dk);
    if (jtruth(dk)) {
        json_t *key0 = dk_first(dk);
        OR2_PROP(key0);
        json_t *a = json_object();
        aset_j(a, "key", key0);
        json_free(key0);
        json_t *rec = or2_call(sp, "store.get", a);
        json_free(a);
        OR2_PROP(rec);
        a = json_object();
        aset_j(a, "record", rec);
        json_free(rec);
        json_t *st = or2_call(sp, "settled", a);
        json_free(a);
        OR2_PROP(st);
        bool settled = jtruth(st);
        json_free(st);
        if (settled) return NULL;
    }
    if (!is_obj(response_env)) return raise_attr(response_env, "get");
    const json_t *status = json_obj_get(response_env, "status");
    json_t *tsp = sattr(state, "terminal_snapshot_persisted");
    OR2_PROP(tsp);
    if (str_eq(status, "incomplete") && jtruth(tsp)) return NULL;
    json_t *conv = sattr(state, "conversation");
    OR2_PROP(conv);
    json_t *tr = sattr(state, "terminal_replay");
    OR2_PROP(tr);
    bool advance = false;
    if (jtruth(conv)) {
        if (!jtruth(tr)) {
            advance = true;
        } else {
            json_t *a = json_object();
            aset_j(a, "conversation", conv);
            json_t *gc = or2_call(sp, "store.get_conversation", a);
            json_free(a);
            OR2_PROP(gc);
            json_t *rid = sattr(state, "response_id");
            if (or2_is_raise(rid)) { json_free(gc); return rid; }
            advance = !jpresent(gc) || py_eq(gc, rid);
            json_free(gc);
        }
    }
    json_t *rid = sattr(state, "response_id");
    OR2_PROP(rid);
    json_t *instr = sattr(state, "instructions");
    OR2_PROP(instr);
    json_t *sid_self = sattr(state, "session_id");
    OR2_PROP(sid_self);
    json_t *hist = NULL;
    bool hist_owned = false;
    if (!jpresent(history)) {
        hist = or2_rs_history_with_user(state);
        OR2_PROP(hist);
        hist_owned = true;
    } else {
        hist = (json_t *)history;
    }
    json_t *value = json_object();
    json_set(value, "response", json_copy(response_env));
    json_set(value, "conversation_history", json_copy(hist));
    json_set(value, "instructions", instr ? json_copy(instr) : json_null());
    json_set(value, "session_id",
             jtruth(session_id) ? json_copy(session_id) : json_copy(sid_self));
    if (hist_owned) json_free(hist);
    json_t *a = json_object();
    aset_j(a, "key", rid);
    json_set(a, "value", value);
    json_t *pr = or2_call(sp, "store.put", a);
    json_free(a);
    OR2_PROP(pr);
    json_free(pr);
    if (advance) {
        a = json_object();
        aset_j(a, "conversation", conv);
        aset_j(a, "response_id", rid);
        json_t *sc = or2_call(sp, "store.set_conversation", a);
        json_free(a);
        OR2_PROP(sc);
        json_free(sc);
    }
    if (!str_eq(status, "in_progress"))
        json_set(state, "terminal_snapshot_persisted", json_bool(true));
    return NULL;
}

/* PoP: or2_rs_commit_terminal @ gateway/platforms/api_server_openai_routes.py:_commit_terminal */
json_t *or2_rs_commit_terminal(json_t *state, const json_t *env, const json_t *full_history,
                               const json_t *session_id, const json_t *record, const or2_seams_t *sp) {
    json_t *store_flag = sattr(state, "store");
    OR2_PROP(store_flag);
    if (jtruth(store_flag)) {
        json_t *r = or2_rs_persist_snapshot(state, env, full_history, session_id, sp);
        OR2_PROP(r);
        json_free(r);
    }
    json_t *dk = sattr(state, "durable_key");
    OR2_PROP(dk);
    if (!jtruth(dk)) return NULL;
    if (jpresent(record)) {
        json_t *key0 = dk_first(dk);
        OR2_PROP(key0);
        json_t *a = json_object();
        aset_j(a, "key", key0);
        json_free(key0);
        aset_j(a, "value", record);
        json_t *pr = or2_call(sp, "store.put", a);
        json_free(a);
        OR2_PROP(pr);
        json_free(pr);
    }
    json_t *key0 = dk_first(dk);
    OR2_PROP(key0);
    json_t *a = json_object();
    aset_j(a, "key", key0);
    json_free(key0);
    json_t *r = or2_call(sp, "store.get", a);
    json_free(a);
    return r;
}

/* PoP: or2_rs_write_event @ gateway/platforms/api_server_openai_routes.py:write_event */
json_t *or2_rs_write_event(json_t *state, const json_t *event_type, const json_t *data,
                           const or2_seams_t *sp) {
    json_t *dk = sattr(state, "durable_key");
    OR2_PROP(dk);
    /* the live def mutates the caller's data dict; the harness echoes the
     * pristine input, so work on a private copy. */
    json_t *d = data ? json_copy(data) : NULL;
    if (!is_obj(d)) {
        char *m = xsf("argument of type '%s' is not a container or iterable", py_type_name(d));
        json_t *r = raise_type(m);
        free(m);
        json_free(d);
        return r;
    }
    if (jtruth(dk) && json_has(d, "output_index")) {
        json_t *sup = sattr(state, "_suppress_incremental");
        if (or2_is_raise(sup)) { json_free(d); return sup; }
        if (jtruth(sup)) { json_free(d); return NULL; }
        const json_t *index = json_obj_get(d, "output_index");
        json_t *pub = sattr(state, "_published_output_ids");
        if (or2_is_raise(pub)) { json_free(d); return pub; }
        size_t pubn = is_arr(pub) ? pub->c.count : 0;
        bool valid = false;
        if (str_eq(event_type, "response.output_item.added")) {
            double iv = (is_num(index) || is_bool(index))
                        ? (is_bool(index) ? (index->bool_val ? 1.0 : 0.0) : index->num_val)
                        : NAN;
            valid = !isnan(iv) && iv == (double)pubn;
            if (valid) {
                const json_t *item = json_obj_get(d, "item");
                if (!item) { json_free(d); return raise_key("item"); }
                if (!is_obj(item)) {
                    char *m = xsf("'%s' object is not subscriptable", py_type_name(item));
                    json_t *r = raise_type(m);
                    free(m);
                    json_free(d);
                    return r;
                }
                const json_t *id = json_obj_get(item, "id");
                if (!id) { json_free(d); return raise_key("id"); }
                json_append(pub, json_copy(id));
            }
        } else {
            const json_t *identity = json_obj_get(d, "item_id");
            json_t *iempty = NULL;
            if (!jtruth(identity)) {
                const json_t *item = json_obj_get(d, "item");
                if (!item) { iempty = json_object(); item = iempty; }
                if (!is_obj(item)) {
                    json_t *r = raise_attr(item, "get");
                    json_free(iempty); json_free(d);
                    return r;
                }
                identity = json_obj_get(item, "id");
            }
            json_free(iempty);
            if (!(is_num(index) || is_bool(index))) {
                char *m = xsf("'<=' not supported between instances of 'int' and '%s'",
                              py_type_name(index));
                json_t *r = raise_type(m);
                free(m);
                json_free(d);
                return r;
            }
            double iv = is_bool(index) ? (index->bool_val ? 1.0 : 0.0) : index->num_val;
            valid = iv >= 0 && iv < (double)pubn;
            if (valid && jpresent(identity))
                valid = py_eq(pub->c.items[(size_t)iv], identity);
        }
        if (!valid) {
            json_set(state, "_suppress_incremental", json_bool(true));
            json_free(d);
            return NULL;
        }
    }
    json_t *seq = sattr(state, "sequence_number");
    if (or2_is_raise(seq)) { json_free(d); return seq; }
    if (!json_has(d, "sequence_number"))
        json_set(d, "sequence_number", json_copy(seq));
    if (!(is_num(seq) || is_bool(seq))) {
        char *m = xsf("unsupported operand type(s) for +: '%s' and 'int'", py_type_name(seq));
        json_t *r = raise_type(m);
        free(m);
        json_free(d);
        return r;
    }
    double sv = is_bool(seq) ? (seq->bool_val ? 1.0 : 0.0) : seq->num_val;
    json_set(state, "sequence_number", json_number(sv + 1));
    if (jtruth(dk)) {
        json_t *re = sattr(state, "recorded_events");
        if (or2_is_raise(re)) { json_free(d); return re; }
        json_t *ev = json_array();
        json_append(ev, event_type ? json_copy(event_type) : json_null());
        json_append(ev, json_copy(d));
        json_append(re, ev);
    }
    json_t *a = json_object();
    aset_j(a, "data", d);
    aset_j(a, "event", event_type);
    json_t *frame = or2_call(sp, "sse_frame", a);
    json_free(a);
    if (or2_is_raise(frame)) { json_free(d); return frame; }
    a = json_object();
    aset_j(a, "data", frame);
    json_t *w = or2_call(sp, "response.write", a);
    json_free(a);
    json_free(frame);
    json_free(d);
    OR2_PROP(w);
    json_free(w);
    return NULL;
}