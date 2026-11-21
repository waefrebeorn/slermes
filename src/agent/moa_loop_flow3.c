/*
 * moa_loop_flow3.c — FLOW port of the REMAINING unclaimed lifecycle core of
 * agent/moa_loop.py (quarry: C:/Users/eman5/hermes-agent, LIVE authority).
 *
 * Claim landscape (grep PoP by PYTHON name first, CRLF-aware):
 *   - moa_loop_flow.c (flow1) faithfully ports 18 fan-out/aggregation cores;
 *     moa_loop_flow2.c (flow2) ports 20 redaction/preset/relay/property defs.
 *     Those claimed siblings are SEAMED here.
 *   - Every remaining def of agent/moa_loop.py carries a PoP claim ONLY from
 *     printf-stub-era fakes that never gated live behaviour (flow2 header
 *     doctrine: a real port of the same (module,func) pair is the sanctioned
 *     revival that lets the stub die):
 *       moa_loop_wrappers.c  moa_u_* / moa_* tab-arg printf stubs
 *       moa_loop_ports.c     moa_init printf JSON; "catalog" PoP lines over
 *                            unrelated provider-catalog code (create /
 *                            _call_prepared_aggregator)
 *       moa_loop_gaps3.c     last_reference_metrics strdup("") stub
 *       gw_dispatch.c        gw_event_emit (gateway event bus — a DIFFERENT
 *                            function; the PoP misattributes MoAChatCompletions
 *                            ._emit, whose live body dispatches
 *                            reference_callback and swallows Exception)
 *   - 15 targets: the MoAChatCompletions lifecycle (10 methods) + the
 *     MoAClient delegation surface (5 methods). Ported functions compose REAL
 *     (internal composition: prepare->create->_call_prepared_aggregator,
 *     MoAClient.*->MoAChatCompletions.*, MoAClient.__init__->
 *     MoAChatCompletions.__init__); every claimed sibling / foreign import /
 *     callable double rides the injected universal seam ml3_io_t.call.
 *
 * ── Universal seam contract ────────────────────────────────────────────────
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 * args is OWNED by ml3_call (freed after the call); the result is a NEW
 * json_t owned by the caller: NULL or a JSON_NULL node == Python None; an
 * object carrying a string "__raise__" key ({"__raise__": cls, "msg": str})
 * == an exception of that class with str(exc) == msg, propagated or caught
 * exactly where the live code does (BaseException-only kinds pass through
 * every `except Exception` / suppress(Exception) site). The harness records
 * one {ev, args, ret} event per call (ret = RAW scripted value) in call order.
 *
 * Script shapes (fixture "io" object, per seam name):
 *   absent / null        -> None on every call
 *   {"seq": [v1, v2..]}  -> vi on the i-th call (0-based); None when exhausted
 *   any other value      -> reused on every call
 *
 * ── Seam name table (name -> live call) ────────────────────────────────────
 *   _sum_reference_accounting        flow1-claimed module sibling
 *   _fold_pending_accounting         flow1-claimed SELF method (strict sig)
 *   _fanout_cache_key                flow1-claimed SELF method
 *   _run_fanout                      flow1-claimed SELF method
 *   _build_guidance                  flow1-claimed SELF method (stale kwarg)
 *   _tool_activity_since_last_user   flow1-claimed module sibling
 *   _resolve_preset_cached           flow2-claimed module sibling
 *   _moa_privacy_mode                flow2-claimed module sibling
 *   _attach_reference_guidance       flow2-claimed module sibling
 *   _redact_trace_messages           flow2-claimed module sibling
 *   _slot_label                      flow2-claimed module sibling
 *   _plan_aggregator_cache           flow2-claimed SELF method
 *   _reference_messages              stub-claimed module sibling
 *   _preset_temperature              stub-claimed module sibling
 *   _slot_runtime                    stub-claimed module sibling
 *   _merge_slot_extra_body           stub-claimed module sibling
 *   _aggregator_reasoning_config     stub-claimed module sibling
 *   _extract_text                    stub-claimed module sibling
 *   _completed_response_as_stream_chunk  stub-claimed module sibling
 *   call_llm                         agent.auxiliary_client.call_llm (partial)
 *   destination_key                  agent.moa_alternation.destination_key
 *   merge_same_role_messages         agent.moa_alternation (identity: a ret
 *                                    {"__echo__":"messages"} means the live
 *                                    callee returned its input object itself)
 *   is_role_alternation_rejection    agent.moa_alternation (exc arg = str(exc))
 *   attempt_facts                    agent.prompt_cache_clock.attempt_facts
 *   save_moa_turn                    agent.moa_trace.save_moa_turn
 *   reference_callback               self.reference_callback callable double
 *
 * ── Value model ────────────────────────────────────────────────────────────
 * None            -> JSON null (pointer NULL and JSON_NULL node both read None).
 * dict            -> plain JSON object (no __attr__ tag).
 * attribute object-> {"__attr__": {name: value}} (SimpleNamespace double).
 * callable double -> {"__call__": "<seam name>"}; calling it fires that seam.
 * _RefAccounting  -> {"_type": "_RefAccounting", usage, cost_usd, cost_status,
 *                    cost_source, messages, output, model, provider,
 *                    temperature} (full field set; absent fields read None).
 * CanonicalUsage  -> INPUT tag {"_type": "CanonicalUsage", <fields>}; a live
 *                    instance is ALWAYS truthy (no __bool__/__len__). OUTPUT
 *                    projection is the plain asdict field dict (no tag):
 *                    input_tokens/output_tokens/cache_read_tokens/
 *                    cache_write_tokens/reasoning_tokens 0, request_count 1,
 *                    raw_usage null, cache_write_by_ttl [].
 * tuple           -> JSON array. set -> SORTED JSON array (both sides sort).
 * one-chunk iter  -> {"__iter__": [chunk]} (stream+choices return path).
 * integral floats normalize to ints on both sides (canon).
 *
 * ── State window ───────────────────────────────────────────────────────────
 * MoAChatCompletions.* : state IS the facade attribute window (the self
 *   SimpleNamespace double). threading.Lock (_accounting_lock) is
 *   unobservable single-threaded: the oracle injects a NoopLock and BOTH
 *   sides strip it (and the bound-method keys) from the emitted window.
 *   __init__ builds a FRESH window (in.state ignored).
 * MoAClient.* : state is the client window {"chat": {"completions": <facade
 *   window>}}; chat/completions are direct attribute reads (AttributeError
 *   when absent / wrong carrier), the delegated method rides the facade
 *   window in place. __init__ builds a FRESH window.
 * _pending_trace / last_aggregator_route / last_aggregator_slot /
 *   last_aggregator_attempt / _privacy_mode / _merge_same_role_destinations
 *   carry partial mutations on a raise (the harness emits state as-is).
 *
 * ── Fixture domain (ASCII scope) ───────────────────────────────────────────
 * str() operands are scalars; messages / agg_messages / reference_outputs
 * rows are lists of dicts / [label,text,acct] triples; preset / agg_runtime /
 * prepared / api_kwargs / pending are plain dicts or null; _pending_reference_
 * usage is null | CanonicalUsage tag | plain dict (live dict truthiness);
 * _ref_cache_key is null | array (tuple tree); _ref_cache_outputs null | array;
 * _merge_same_role_destinations null | array of [route,model] arrays (set);
 * destination scripts are arrays (materialized to tuples live) or hashable
 * scalars — a dict script probes the py3.14 unhashable-set-element raise;
 * _sum_reference_accounting scripts are arrays | null | int (the * unpack
 * domain); agg_runtime keys stay disjoint from the partial's explicit kwargs
 * except the one documented duplicate-key probe; logger is SILENT (no seam
 * events) but logger ARGUMENTS still evaluate (_slot_label fires inside the
 * retry warning).
 *
 * Self-contained: libjson only (gate links lib/libjson/json.o). The harness
 * #includes "agent/moa_loop_flow3.c" directly. All symbols static ml3_*.
 */
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

/* ── JSON / Python-value helpers ─────────────────────────────────────────── */

static bool ml3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool ml3_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ml3_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ml3_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ml3_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ml3_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

/* isinstance(v, dict): plain object only (an __attr__ double is NOT a dict). */
static bool ml3_is_dict(const json_t *v) {
    return ml3_is_obj(v) && !json_obj_get(v, "__attr__");
}
static const json_t *ml3_attr(const json_t *v) {
    if (!ml3_is_obj(v)) return NULL;
    const json_t *a = json_obj_get(v, "__attr__");
    return ml3_is_obj(a) ? a : NULL;
}
static bool ml3_is_callable(const json_t *v) {
    return ml3_is_obj(v) && ml3_is_str(json_obj_get(v, "__call__"));
}
static bool ml3_is_acct(const json_t *v) {
    return ml3_is_obj(v) && ml3_is_str(json_obj_get(v, "_type"))
        && strcmp(json_obj_get(v, "_type")->str_val, "_RefAccounting") == 0;
}
static bool ml3_is_cu(const json_t *v) {
    return ml3_is_obj(v) && ml3_is_str(json_obj_get(v, "_type"))
        && strcmp(json_obj_get(v, "_type")->str_val, "CanonicalUsage") == 0;
}

/* Python truthiness (NULL pointer == None). A CanonicalUsage tag is ALWAYS
 * truthy (live dataclass without __bool__/__len__); a plain dict follows
 * count truthiness. */
static bool ml3_truth(const json_t *v) {
    if (!v) return false;
    if (ml3_is_cu(v)) return true;
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

/* Python type name of a JSON value (ASCII scalar domain). */
static const char *ml3_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return ml3_attr(v) ? "types.SimpleNamespace" : "dict";
    }
    return "object";
}

static void ml3_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() of a scalar (None/bool/str/number). malloc'd. */
static char *ml3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                ml3_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: {
            /* Outside the scalar str() domain: serialize (documented
             * exclusion for containers; kept fail-safe). */
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

/* str(x or def) — def when x is falsy/absent. malloc'd. */
static char *ml3_pystr_or(const json_t *v, const char *def) {
    if (!ml3_truth(v)) return strdup(def);
    return ml3_pystr(v);
}

/* ── exception model ─────────────────────────────────────────────────────── */

static void ml3_raise(json_t **exc, const char *cls, const char *msg) {
    if (*exc) return;   /* first raise wins */
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    *exc = r;
}
static void ml3_raise_fmt(json_t **exc, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ml3_raise(exc, cls, buf);
}

/* AttributeError: '<T>' object has no attribute '<name>' */
static void ml3_attr_err(json_t **exc, const json_t *carrier, const char *name) {
    ml3_raise_fmt(exc, "AttributeError", "'%s' object has no attribute '%s'",
                  ml3_type_name(carrier), name);
}
/* The facade/client self doubles are SimpleNamespace; their windows are
 * plain JSON objects, so attribute errors name the double type explicitly. */
static void ml3_ns_attr_err(json_t **exc, const char *name) {
    ml3_raise_fmt(exc, "AttributeError",
                  "'types.SimpleNamespace' object has no attribute '%s'", name);
}

/* FULL builtin exception hierarchy (py3.14): name -> parent. A scripted kind
 * walks this table; reaching Exception == caught by `except Exception`,
 * reaching BaseException without Exception == propagates. Unknown names are
 * synthetic Exception subclasses (the oracle's make_exc builds them so).
 * ExceptionGroup multiply-inherits (BaseExceptionGroup, Exception): the
 * Exception edge models the catch behaviour. EnvironmentError/IOError/
 * WindowsError are OSError aliases. CancelledError is asyncio's
 * BaseException-only kind. */
typedef struct { const char *name; const char *parent; } ml3_exc_parent_t;
static const ml3_exc_parent_t ML3_EXC_PARENTS[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"TypeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
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
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
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
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
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
};

static bool ml3_exc_is_exception(const char *cls) {
    if (!cls) return true;
    for (int depth = 0; depth < 16; depth++) {
        if (strcmp(cls, "Exception") == 0) return true;
        if (strcmp(cls, "BaseException") == 0) return false;
        const char *parent = NULL;
        bool found = false;
        for (size_t i = 0; i < sizeof(ML3_EXC_PARENTS) / sizeof(ML3_EXC_PARENTS[0]); i++) {
            if (strcmp(ML3_EXC_PARENTS[i].name, cls) == 0) {
                parent = ML3_EXC_PARENTS[i].parent;
                found = true;
                break;
            }
        }
        if (!found) return true;   /* synthetic type(cls, (Exception,), {}) */
        if (!parent) return false; /* BaseException root */
        cls = parent;
    }
    return true;
}

/* Swallow a caught exception (live `except Exception` / suppress(Exception)).
 * BaseException-only kinds propagate: returns false, leaves *exc set. */
static bool ml3_swallow(json_t **exc) {
    if (!*exc) return true;
    const char *cls = json_get_str(*exc, "__raise__", "");
    if (!ml3_exc_is_exception(cls)) return false;
    json_free(*exc); *exc = NULL;
    return true;
}

/* ── universal seam ──────────────────────────────────────────────────────── */

typedef struct ml3_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ml3_io_t;

/* args builders (None-valued keys dropped on both sides). */
static void ml3_arg_j(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void ml3_arg_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void ml3_arg_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}

/* Universal seam call: frees args, returns the NEW scripted result (NULL ==
 * None) or transfers a raise-spec into *exc. */
static json_t *ml3_call(const ml3_io_t *io, const char *name, json_t *args,
                        json_t **exc) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__")) {
        if (!*exc) *exc = r;
        else json_free(r);
        return NULL;
    }
    return r;
}

/* Fire a callable double ({"__call__": name}); a non-callable non-None value
 * raises the live TypeError '<T>' object is not callable. */
static json_t *ml3_call_double(const json_t *fn, const ml3_io_t *io,
                               json_t *args, json_t **exc) {
    if (ml3_is_callable(fn)) {
        const char *name = json_obj_get(fn, "__call__")->str_val;
        return ml3_call(io, name, args, exc);
    }
    ml3_raise_fmt(exc, "TypeError", "'%s' object is not callable",
                  ml3_type_name(fn));
    json_free(args);
    return NULL;
}

/* ── dict / attribute / subscript navigation ─────────────────────────────── */

/* dict.get on a value that must be a dict (AttributeError otherwise). */
static const json_t *ml3_dict_get(const json_t *d, const char *key,
                                  json_t **exc) {
    if (!ml3_is_dict(d)) {
        ml3_attr_err(exc, d, "get");
        return NULL;
    }
    return json_obj_get(d, key);
}

/* dict.pop(key, None) on a value that must be a dict. Returns an OWNED copy
 * (or NULL == None) and removes the key in place. */
static json_t *ml3_dict_pop(json_t *d, const char *key, json_t **exc) {
    if (!ml3_is_dict(d)) {
        ml3_attr_err(exc, d, "pop");
        return NULL;
    }
    const json_t *v = json_obj_get(d, key);
    json_t *out = v ? json_copy(v) : NULL;
    json_obj_del(d, key);
    return out;
}

/* d[key] — KeyError when absent; the live TypeError shapes otherwise. */
static const json_t *ml3_subscript(const json_t *d, const char *key,
                                   json_t **exc) {
    if (ml3_is_dict(d)) {
        const json_t *v = json_obj_get(d, key);
        if (!v) ml3_raise_fmt(exc, "KeyError", "'%s'", key);
        return v;
    }
    if (ml3_is_arr(d)) {
        ml3_raise(exc, "TypeError",
                  "list indices must be integers or slices, not str");
        return NULL;
    }
    if (ml3_is_str(d)) {
        ml3_raise(exc, "TypeError",
                  "string indices must be integers, not 'str'");
        return NULL;
    }
    ml3_raise_fmt(exc, "TypeError", "'%s' object is not subscriptable",
                  ml3_type_name(d));
    return NULL;
}

/* self.<name> direct read on the facade/client window (plain JSON object ==
 * SimpleNamespace double). Absent -> AttributeError naming the double. */
static const json_t *ml3_self_get(const json_t *state, const char *name,
                                  json_t **exc) {
    const json_t *v = json_obj_get(state, name);
    if (!v) ml3_ns_attr_err(exc, name);
    return v;
}
/* getattr(self, name, None) — absent reads NULL (the caller applies its own
 * default); present-null reads the null node. */
static const json_t *ml3_self_get_def(const json_t *state, const char *name) {
    return json_obj_get(state, name);
}

/* hasattr(v, name): only an __attr__ double carries attributes; a plain dict
 * / scalar / None has none (live dict has no .choices etc.). */
static bool ml3_hasattr(const json_t *v, const char *name) {
    const json_t *a = ml3_attr(v);
    return a && json_obj_get(a, name) != NULL;
}

/* Assignment unpack of a seam result into exactly n items (list domain). */
static bool ml3_unpack(const json_t *r, size_t n, const json_t **items,
                       json_t **exc) {
    if (!ml3_is_arr(r)) {
        ml3_raise_fmt(exc, "TypeError", "cannot unpack non-iterable %s object",
                      ml3_type_name(r));
        return false;
    }
    if (r->c.count > n) {
        ml3_raise_fmt(exc, "ValueError",
                      "too many values to unpack (expected %zu, got %zu)",
                      n, r->c.count);
        return false;
    }
    if (r->c.count < n) {
        ml3_raise_fmt(exc, "ValueError",
                      "not enough values to unpack (expected %zu, got %zu)",
                      n, r->c.count);
        return false;
    }
    for (size_t i = 0; i < n; i++) items[i] = r->c.items[i];
    return true;
}

/* [dict(m) for m in messages] — list-of-dicts domain. Non-list carrier or
 * non-dict element raises the live TypeError (scalar/null elements). */
static json_t *ml3_msg_copies(const json_t *messages, json_t **exc) {
    if (!ml3_is_arr(messages)) {
        ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                      ml3_type_name(messages));
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *m = messages->c.items[i];
        if (!ml3_is_dict(m)) {
            ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                          ml3_type_name(m));
            json_free(out);
            return NULL;
        }
        json_append(out, json_copy(m));
    }
    return out;
}

/* ── CanonicalUsage / _RefAccounting projections ─────────────────────────── */

static const char *ML3_CU_FIELDS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "request_count", "raw_usage", "cache_write_by_ttl",
};
static const char *ML3_ACCT_FIELDS[] = {
    "usage", "cost_usd", "cost_status", "cost_source", "messages", "output",
    "model", "provider", "temperature",
};

/* Fresh CanonicalUsage() asdict projection (plain field dict, no tag). */
static json_t *ml3_cu_new(void) {
    json_t *o = json_object();
    for (size_t i = 0; i < 5; i++)
        json_set(o, ML3_CU_FIELDS[i], json_number(0));
    json_set(o, "request_count", json_number(1));
    json_set(o, "raw_usage", json_null());
    json_set(o, "cache_write_by_ttl", json_array());
    return o;
}

/* Project a _pending_reference_usage value the way norm_ret(asdict()) does:
 * a CanonicalUsage tag -> full plain field dict (defaults fill); any other
 * value passes through unchanged (live dict/scalar semantics). OWNED. */
static json_t *ml3_cu_project(const json_t *v) {
    if (!ml3_is_cu(v)) return v ? json_copy(v) : json_null();
    json_t *o = json_object();
    for (size_t i = 0; i < 5; i++) {
        const json_t *fv = json_obj_get(v, ML3_CU_FIELDS[i]);
        json_set(o, ML3_CU_FIELDS[i], fv ? json_copy(fv) : json_number(0));
    }
    const json_t *rc = json_obj_get(v, "request_count");
    json_set(o, "request_count", rc ? json_copy(rc) : json_number(1));
    const json_t *ru = json_obj_get(v, "raw_usage");
    json_set(o, "raw_usage", ru ? json_copy(ru) : json_null());
    const json_t *ct = json_obj_get(v, "cache_write_by_ttl");
    json_set(o, "cache_write_by_ttl", ct ? json_copy(ct) : json_array());
    return o;
}

/* Fill a tagged _RefAccounting to the full field set (materialize_acct ->
 * norm_ret round trip). Non-tagged values pass through. OWNED. */
static json_t *ml3_acct_fill(const json_t *v) {
    if (!ml3_is_acct(v)) return v ? json_copy(v) : json_null();
    json_t *o = json_object();
    json_set(o, "_type", json_string("_RefAccounting"));
    for (size_t i = 0; i < 9; i++) {
        const json_t *fv = json_obj_get(v, ML3_ACCT_FIELDS[i]);
        json_set(o, ML3_ACCT_FIELDS[i], fv ? json_copy(fv) : json_null());
    }
    return o;
}

/* ── equality / set helpers ──────────────────────────────────────────────── */

/* Python == over the JSON domain (bool/num cross-compare numerically). */
static bool ml3_deep_eq(const json_t *a, const json_t *b) {
    bool an = ml3_is_null(a), bn = ml3_is_null(b);
    if (an || bn) return an && bn;
    bool anum = ml3_is_num(a) || ml3_is_bool(a);
    bool bnum = ml3_is_num(b) || ml3_is_bool(b);
    if (anum || bnum) {
        if (!anum || !bnum) return false;
        double av = ml3_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = ml3_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "",
                          b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ml3_deep_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !ml3_deep_eq(a->c.items[i], bv)) return false;
            }
            return true;
        }
        default: return false;
    }
}

/* Lexicographic order over the JSON domain (set sorting; homogeneous
 * [route,model] string arrays in the fixture domain). */
static int ml3_json_cmp(const json_t *a, const json_t *b) {
    bool an = ml3_is_null(a), bn = ml3_is_null(b);
    if (an || bn) return (an ? 0 : 1) - (bn ? 0 : 1);
    bool anum = ml3_is_num(a) || ml3_is_bool(a);
    bool bnum = ml3_is_num(b) || ml3_is_bool(b);
    if (anum && bnum) {
        double av = ml3_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = ml3_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return (av > bv) - (av < bv);
    }
    if (a->type != b->type) return (int)a->type - (int)b->type;
    switch (a->type) {
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "",
                          b->str_val ? b->str_val : "");
        case JSON_ARRAY: {
            size_t n = a->c.count < b->c.count ? a->c.count : b->c.count;
            for (size_t i = 0; i < n; i++) {
                int c = ml3_json_cmp(a->c.items[i], b->c.items[i]);
                if (c) return c;
            }
            return (a->c.count > b->c.count) - (a->c.count < b->c.count);
        }
        default: {
            char *sa = json_serialize(a), *sb = json_serialize(b);
            int c = strcmp(sa ? sa : "", sb ? sb : "");
            free(sa); free(sb);
            return c;
        }
    }
}

static bool ml3_arr_contains(const json_t *arr, const json_t *v) {
    if (!ml3_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (ml3_deep_eq(arr->c.items[i], v)) return true;
    return false;
}

/* set.add: append when absent, then keep the array sorted (both sides emit
 * sets sorted). Mutates arr in place. */
static void ml3_set_add(json_t *arr, const json_t *v) {
    if (!ml3_is_arr(arr) || ml3_arr_contains(arr, v)) return;
    json_append(arr, json_copy(v));
    /* insertion sort by ml3_json_cmp */
    for (size_t i = arr->c.count; i > 1; i--) {
        json_t *cur = arr->c.items[i - 1];
        if (ml3_json_cmp(arr->c.items[i - 2], cur) <= 0) break;
        arr->c.items[i - 1] = arr->c.items[i - 2];
        arr->c.items[i - 2] = cur;
    }
}

/* Sort a set-valued state array in place (rebuild). */
static void ml3_sort_array(json_t *arr) {
    if (!ml3_is_arr(arr) || arr->c.count < 2) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        json_t *cur = arr->c.items[i];
        size_t j = i;
        while (j > 0 && ml3_json_cmp(arr->c.items[j - 1], cur) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = cur;
    }
}

/* ── state-window normalization (harness applies before emit) ────────────── */

static const char *ML3_FACADE_BINDS[] = {
    "__init__", "consume_reference_usage", "last_reference_metrics",
    "_record_late_reference_accounting", "consume_and_save_trace", "_emit",
    "prepare", "rebase_prepared_request", "create", "_call_prepared_aggregator",
    "_fold_pending_accounting", "_fanout_cache_key", "_run_fanout",
    "_build_guidance", "_plan_aggregator_cache",
};

/* Normalize a facade window the way the oracle's materialize->norm round
 * trip does: strip the unobservable lock + bound-method keys, project a
 * CanonicalUsage-tagged _pending_reference_usage to its asdict field dict,
 * fill tagged _RefAccounting elements of _ref_cache_outputs rows, sort the
 * _merge_same_role_destinations set array. Mutates in place. */
static void ml3_norm_facade(json_t *state) {
    if (!ml3_is_obj(state)) return;
    json_obj_del(state, "_accounting_lock");
    for (size_t i = 0; i < sizeof(ML3_FACADE_BINDS) / sizeof(ML3_FACADE_BINDS[0]); i++)
        json_obj_del(state, ML3_FACADE_BINDS[i]);
    const json_t *pu = json_obj_get(state, "_pending_reference_usage");
    if (ml3_is_cu(pu))
        json_set(state, "_pending_reference_usage", ml3_cu_project(pu));
    json_t *rco = json_obj_get(state, "_ref_cache_outputs");
    if (ml3_is_arr(rco)) {
        for (size_t i = 0; i < rco->c.count; i++) {
            json_t *row = rco->c.items[i];
            if (!ml3_is_arr(row)) continue;
            for (size_t j = 0; j < row->c.count; j++) {
                if (ml3_is_acct(row->c.items[j])) {
                    json_t *filled = ml3_acct_fill(row->c.items[j]);
                    json_free(row->c.items[j]);
                    row->c.items[j] = filled;
                }
            }
        }
    }
    json_t *msrd = json_obj_get(state, "_merge_same_role_destinations");
    if (ml3_is_arr(msrd)) ml3_sort_array(msrd);
}

/* ── 1. MoAChatCompletions.__init__ ─────────────────────────────────────────
 *   self.preset_name = preset_name or "default"
 *   self.reference_callback = reference_callback
 *   self._agent = agent
 *   self._ref_cache_key = None; self._ref_cache_outputs = []
 *   self._pending_reference_usage = CanonicalUsage()
 *   self._pending_reference_cost = None
 *   self._accounting_lock = threading.Lock()   (unobservable; omitted)
 *   self.last_aggregator_slot = None
 *   self.last_aggregator_route = None; self.last_aggregator_attempt = None
 *   self._pending_trace = None; self._last_reference_metrics = None
 *   self._fanout_iteration_count = 0
 *   self._fanout_turn_sig = None; self._fanout_last_state_sig = None
 *   self._privacy_mode = ""
 *   self._merge_same_role_destinations = set()
 * The harness hands a FRESH state window (live constructs a new object).
 */
static json_t *ml3_mcc_init(const json_t *in, const ml3_io_t *io,
                            json_t *state, json_t **exc) {
    (void)io; (void)exc;
    const json_t *pn = json_obj_get(in, "preset_name");
    json_set(state, "preset_name", ml3_truth(pn) ? json_copy(pn)
                                                 : json_string("default"));
    const json_t *rc = json_obj_get(in, "reference_callback");
    json_set(state, "reference_callback", rc ? json_copy(rc) : json_null());
    const json_t *ag = json_obj_get(in, "agent");
    json_set(state, "_agent", ag ? json_copy(ag) : json_null());
    json_set(state, "_ref_cache_key", json_null());
    json_set(state, "_ref_cache_outputs", json_array());
    json_set(state, "_pending_reference_usage", ml3_cu_new());
    json_set(state, "_pending_reference_cost", json_null());
    json_set(state, "last_aggregator_slot", json_null());
    json_set(state, "last_aggregator_route", json_null());
    json_set(state, "last_aggregator_attempt", json_null());
    json_set(state, "_pending_trace", json_null());
    json_set(state, "_last_reference_metrics", json_null());
    json_set(state, "_fanout_iteration_count", json_number(0));
    json_set(state, "_fanout_turn_sig", json_null());
    json_set(state, "_fanout_last_state_sig", json_null());
    json_set(state, "_privacy_mode", json_string(""));
    json_set(state, "_merge_same_role_destinations", json_array());
    return json_null();
}

/* ── 2. MoAChatCompletions.consume_reference_usage ──────────────────────────
 *   with self._accounting_lock:
 *       usage = self._pending_reference_usage or CanonicalUsage()
 *       cost = self._pending_reference_cost
 *       self._pending_reference_usage = CanonicalUsage()
 *       self._pending_reference_cost = None
 *   return usage, cost
 */
static json_t *ml3_mcc_consume_reference_usage(const json_t *in,
                                               const ml3_io_t *io,
                                               json_t *state, json_t **exc) {
    (void)in; (void)io;
    const json_t *pu = ml3_self_get(state, "_pending_reference_usage", exc);
    if (*exc) return NULL;
    const json_t *pc = ml3_self_get(state, "_pending_reference_cost", exc);
    if (*exc) return NULL;
    json_t *usage = ml3_truth(pu) ? ml3_cu_project(pu) : ml3_cu_new();
    json_t *cost = pc ? json_copy(pc) : json_null();
    json_set(state, "_pending_reference_usage", ml3_cu_new());
    json_set(state, "_pending_reference_cost", json_null());
    json_t *ret = json_array();
    json_append(ret, usage);
    json_append(ret, cost);
    return ret;
}

/* ── 3. MoAChatCompletions.last_reference_metrics ───────────────────────────
 *   return self._last_reference_metrics
 */
static json_t *ml3_mcc_last_reference_metrics(const json_t *in,
                                              const ml3_io_t *io,
                                              json_t *state, json_t **exc) {
    (void)in; (void)io;
    const json_t *m = ml3_self_get(state, "_last_reference_metrics", exc);
    if (*exc) return NULL;
    return m ? json_copy(m) : json_null();
}

/* ── 4. MoAChatCompletions._record_late_reference_accounting ────────────────
 *   if not isinstance(accounting, _RefAccounting): return
 *   self._fold_pending_accounting(*_sum_reference_accounting(
 *       [(label, "", accounting)]))
 *   logger.debug(..., label)   (silent; no seam-valued args)
 * _sum_reference_accounting (flow1) and the self._fold_pending_accounting
 * method (flow1, strict (self, usage, cost) signature) are seamed. The *
 * unpack replicates py3.14 call-binding errors.
 */
static json_t *ml3_mcc_record_late(const json_t *in, const ml3_io_t *io,
                                   json_t *state, json_t **exc) {
    (void)state;
    const json_t *acct = json_obj_get(in, "accounting");
    if (!ml3_is_acct(acct)) return json_null();
    json_t *row = json_array();
    json_append(row, json_string(json_get_str(in, "label", "")));
    json_append(row, json_string(""));
    json_append(row, ml3_acct_fill(acct));
    json_t *outputs = json_array();
    json_append(outputs, row);
    json_t *a = json_object();
    ml3_arg_j(a, "outputs", outputs);
    json_free(outputs);
    json_t *sum = ml3_call(io, "_sum_reference_accounting", a, exc);
    if (*exc) return NULL;
    /* self._fold_pending_accounting(*sum) — py3.14 binding shapes. The
     * *-non-iterable message carries the bound stub's module-qualified name
     * (the oracle extracts it into moa_loop_flow3_extracted); the missing /
     * too-many messages do not carry the prefix. */
    if (!ml3_is_arr(sum)) {
        ml3_raise_fmt(exc, "TypeError",
                      "moa_loop_flow3_extracted._fold_pending_accounting() "
                      "argument after * must be an iterable, not %s",
                      ml3_type_name(sum));
        json_free(sum);
        return NULL;
    }
    size_t n = sum->c.count;
    if (n == 0) {
        ml3_raise(exc, "TypeError",
                  "_fold_pending_accounting() missing 2 required positional "
                  "arguments: 'usage' and 'cost'");
        json_free(sum);
        return NULL;
    }
    if (n == 1) {
        ml3_raise(exc, "TypeError",
                  "_fold_pending_accounting() missing 1 required positional "
                  "argument: 'cost'");
        json_free(sum);
        return NULL;
    }
    if (n > 2) {
        ml3_raise_fmt(exc, "TypeError",
                      "_fold_pending_accounting() takes 3 positional "
                      "arguments but %zu were given", n + 1);
        json_free(sum);
        return NULL;
    }
    json_t *fa = json_object();
    ml3_arg_j(fa, "usage", sum->c.items[0]);
    ml3_arg_j(fa, "cost", sum->c.items[1]);
    json_free(sum);
    json_t *fr = ml3_call(io, "_fold_pending_accounting", fa, exc);
    if (*exc) return NULL;
    json_free(fr);
    return json_null();
}

/* ── 5. MoAChatCompletions.consume_and_save_trace ───────────────────────────
 *   pending = self._pending_trace
 *   self._pending_trace = None
 *   if not pending or "aggregator_input_messages" not in pending: return
 *   try:
 *       from agent.moa_trace import save_moa_turn
 *       agg_slot = pending.get("aggregator_slot") or {}
 *       agg_output = pending.get("aggregator_output")
 *       if agg_output is None and aggregator_output_fallback:
 *           agg_output = aggregator_output_fallback
 *       save_moa_turn(session_id=..., preset_name=pending.get("preset", ""),
 *           reference_outputs=pending.get("reference_outputs", []),
 *           aggregator_label=pending.get("aggregator_label", ""),
 *           aggregator_model=agg_slot.get("model"),
 *           aggregator_provider=agg_slot.get("provider"),
 *           aggregator_temperature=pending.get("aggregator_temperature"),
 *           aggregator_input_messages=pending.get("aggregator_input_messages"),
 *           aggregator_output=agg_output,
 *           aggregator_streamed=bool(pending.get("aggregator_streamed")))
 *   except Exception as exc: logger.debug(...)   (silent)
 */
static json_t *ml3_mcc_consume_and_save_trace(const json_t *in,
                                              const ml3_io_t *io,
                                              json_t *state, json_t **exc) {
    const json_t *pj = ml3_self_get(state, "_pending_trace", exc);
    if (*exc) return NULL;
    /* self._pending_trace = None frees the slot's OLD value; the live local
     * `pending` keeps the dict alive. Snapshot BEFORE the reset or every
     * later pending read is a use-after-free. */
    json_t *pending = pj ? json_copy(pj) : NULL;
    json_set(state, "_pending_trace", json_null());
    if (!ml3_truth(pending)) { json_free(pending); return json_null(); }
    /* "aggregator_input_messages" not in pending — py3.14 containment */
    bool contains = false;
    if (ml3_is_dict(pending)) {
        contains = json_obj_get(pending, "aggregator_input_messages") != NULL;
    } else if (ml3_is_arr(pending)) {
        json_t *needle = json_string("aggregator_input_messages");
        contains = ml3_arr_contains(pending, needle);
        json_free(needle);
    } else if (ml3_is_str(pending)) {
        contains = pending->str_val
            && strstr(pending->str_val, "aggregator_input_messages") != NULL;
    } else {
        ml3_raise_fmt(exc, "TypeError",
                      "argument of type '%s' is not a container or iterable",
                      ml3_type_name(pending));
        json_free(pending);
        return NULL;
    }
    if (!contains) { json_free(pending); return json_null(); }

    json_t *e2 = NULL;
    json_t *agg_slot = NULL;
    json_t *out_v = NULL;
    json_t *sa = NULL;
    const json_t *asv = ml3_dict_get(pending, "aggregator_slot", &e2);
    if (e2) goto caught;
    agg_slot = ml3_truth(asv) ? json_copy(asv) : json_object();
    const json_t *ao = ml3_dict_get(pending, "aggregator_output", &e2);
    if (e2) goto caught;
    out_v = ao ? json_copy(ao) : json_null();
    const json_t *fallback = json_obj_get(in, "aggregator_output_fallback");
    if (ml3_is_null(out_v) && ml3_truth(fallback)) {
        json_free(out_v);
        out_v = json_copy(fallback);
    }
    sa = json_object();
    ml3_arg_j(sa, "session_id", json_obj_get(in, "session_id"));
    {
        const json_t *pv = ml3_dict_get(pending, "preset", &e2);
        if (e2) goto caught;
        if (pv && pv->type != JSON_NULL) ml3_arg_j(sa, "preset_name", pv);
        else if (!pv) ml3_arg_s(sa, "preset_name", "");
        const json_t *ro = ml3_dict_get(pending, "reference_outputs", &e2);
        if (e2) goto caught;
        if (ro && ro->type != JSON_NULL) ml3_arg_j(sa, "reference_outputs", ro);
        else if (!ro) json_set(sa, "reference_outputs", json_array());
        const json_t *al = ml3_dict_get(pending, "aggregator_label", &e2);
        if (e2) goto caught;
        if (al && al->type != JSON_NULL) ml3_arg_j(sa, "aggregator_label", al);
        else if (!al) ml3_arg_s(sa, "aggregator_label", "");
        const json_t *am = ml3_dict_get(agg_slot, "model", &e2);
        if (e2) goto caught;
        ml3_arg_j(sa, "aggregator_model", am);
        const json_t *ap = ml3_dict_get(agg_slot, "provider", &e2);
        if (e2) goto caught;
        ml3_arg_j(sa, "aggregator_provider", ap);
        const json_t *at = ml3_dict_get(pending, "aggregator_temperature", &e2);
        if (e2) goto caught;
        ml3_arg_j(sa, "aggregator_temperature", at);
        const json_t *aim = ml3_dict_get(pending, "aggregator_input_messages", &e2);
        if (e2) goto caught;
        ml3_arg_j(sa, "aggregator_input_messages", aim);
        ml3_arg_j(sa, "aggregator_output", out_v);
        const json_t *as = ml3_dict_get(pending, "aggregator_streamed", &e2);
        if (e2) goto caught;
        ml3_arg_b(sa, "aggregator_streamed", ml3_truth(as));
    }
    {
        json_t *r = ml3_call(io, "save_moa_turn", sa, &e2);
        sa = NULL;   /* ml3_call freed it */
        json_free(r);
        if (e2) goto caught;
    }
    goto done;

caught:
    if (!ml3_swallow(&e2)) {
        *exc = e2;
        json_free(agg_slot); json_free(out_v); json_free(sa);
        json_free(pending);
        return NULL;
    }
done:
    json_free(agg_slot); json_free(out_v); json_free(sa);
    json_free(pending);
    return json_null();
}

/* ── 6. MoAChatCompletions._emit ────────────────────────────────────────────
 *   if self.reference_callback is None: return
 *   try: self.reference_callback(event, **kwargs)
 *   except Exception as exc: logger.debug(...)   (silent)
 */
static json_t *ml3_mcc_emit(const json_t *in, const ml3_io_t *io,
                            json_t *state, json_t **exc) {
    const json_t *cb = ml3_self_get(state, "reference_callback", exc);
    if (*exc) return NULL;
    if (ml3_is_null(cb)) return json_null();
    const json_t *kwargs = json_obj_get(in, "kwargs");
    json_t *a = json_object();
    ml3_arg_s(a, "event", json_get_str(in, "event", ""));
    json_set(a, "kwargs", ml3_is_dict(kwargs) ? json_copy(kwargs)
                                              : json_object());
    json_t *e2 = NULL;
    json_t *r = ml3_call_double(cb, io, a, &e2);
    json_free(r);
    if (e2 && !ml3_swallow(&e2)) {
        *exc = e2;
        return NULL;
    }
    return json_null();
}

/* forward declarations (internal composition) */
static json_t *ml3_mcc_call_prepared_core(const json_t *prepared,
                                          const json_t *ak,
                                          const ml3_io_t *io, json_t *state,
                                          json_t **exc);
static json_t *ml3_mcc_create_core(json_t *ak, const ml3_io_t *io,
                                   json_t *state, json_t **exc);

/* ── 7. MoAChatCompletions.prepare ──────────────────────────────────────────
 *   return self.create(messages=messages, _moa_prepare_only=True)
 */
static json_t *ml3_mcc_prepare(const json_t *in, const ml3_io_t *io,
                               json_t *state, json_t **exc) {
    json_t *ak = json_object();
    const json_t *msgs = json_obj_get(in, "messages");
    json_set(ak, "messages", msgs ? json_copy(msgs) : json_null());
    json_set(ak, "_moa_prepare_only", json_bool(true));
    return ml3_mcc_create_core(ak, io, state, exc);
}

/* ── 8. MoAChatCompletions.rebase_prepared_request ──────────────────────────
 *   guidance = prepared.get("guidance")
 *   agg_messages = [dict(message) for message in messages]
 *   if guidance: _attach_reference_guidance(agg_messages, str(guidance))
 *   return {**prepared, "messages": agg_messages}
 */
static json_t *ml3_mcc_rebase(const json_t *in, const ml3_io_t *io,
                              json_t *state, json_t **exc) {
    (void)state;
    const json_t *prepared = json_obj_get(in, "prepared");
    const json_t *guidance = ml3_dict_get(prepared, "guidance", exc);
    if (*exc) return NULL;
    const json_t *messages = json_obj_get(in, "messages");
    json_t *agg = ml3_msg_copies(messages, exc);
    if (*exc) return NULL;
    if (ml3_truth(guidance)) {
        char *gs = ml3_pystr(guidance);
        json_t *a = json_object();
        ml3_arg_j(a, "agg_messages", agg);
        ml3_arg_s(a, "guidance", gs ? gs : "");
        free(gs);
        json_t *r = ml3_call(io, "_attach_reference_guidance", a, exc);
        if (*exc) { json_free(agg); return NULL; }
        json_free(r);
    }
    json_t *ret = json_copy(prepared);
    json_set(ret, "messages", agg);
    return ret;
}

/* ── 9. MoAChatCompletions.create ───────────────────────────────────────────
 *   prepared_request = api_kwargs.pop("_moa_prepared_request", None)
 *   if prepared_request is not None:
 *       if not isinstance(prepared_request, dict):
 *           raise TypeError("_moa_prepared_request must be a dict")
 *       return self._call_prepared_aggregator(prepared_request, api_kwargs)
 *   preset, moa_raw = _resolve_preset_cached(self.preset_name)
 *   self._privacy_mode = _moa_privacy_mode(moa_raw)
 *   messages = list(api_kwargs.get("messages") or [])
 *   reference_models = [slot for slot in (preset.get("reference_models") or [])
 *       if slot.get("enabled", True)] if preset.get("enabled", True) else []
 *   aggregator = preset.get("aggregator") or {}
 *   self.last_aggregator_slot = dict(aggregator) if aggregator else None
 *   aggregator_temperature = _preset_temperature(preset, "aggregator_temperature")
 *   if aggregator_temperature is None and api_kwargs.get("temperature") is not None:
 *       aggregator_temperature = api_kwargs.get("temperature")
 *   ref_messages = _reference_messages(messages)
 *   cache_key = self._fanout_cache_key(preset, ref_messages, reference_models)
 *   cache_hit = bool(cache_key == self._ref_cache_key and self._ref_cache_outputs)
 *   if cache_hit:
 *       reference_outputs = list(self._ref_cache_outputs)
 *       self._pending_trace = None
 *   else:
 *       reference_outputs = self._run_fanout(preset, ref_messages,
 *           reference_models, aggregator, aggregator_temperature, cache_key)
 *   agg_messages = [dict(m) for m in messages]
 *   guidance = self._build_guidance(reference_outputs, aggregator,
 *       str(preset.get("degraded_reference_policy") or "loud"),
 *       stale=cache_hit and _tool_activity_since_last_user(messages))
 *   if guidance: _attach_reference_guidance(agg_messages, guidance)
 *   prepared_request = {"messages": agg_messages, "guidance": guidance,
 *       "aggregator": aggregator,
 *       "aggregator_temperature": aggregator_temperature}
 *   if api_kwargs.pop("_moa_prepare_only", False): return prepared_request
 *   return self._call_prepared_aggregator(prepared_request, api_kwargs)
 * create_core OWNS ak (the caller's api_kwargs working copy).
 */
static json_t *ml3_mcc_create_core(json_t *ak, const ml3_io_t *io,
                                   json_t *state, json_t **exc) {
    json_t *res = NULL, *messages = NULL, *ref_models = NULL;
    json_t *aggregator = NULL, *at = NULL, *ref_messages = NULL;
    json_t *cache_key = NULL, *reference_outputs = NULL;
    json_t *agg_messages = NULL, *guidance = NULL, *prepared = NULL;
    json_t *ret = NULL;
    char *ds = NULL;

    /* prepared_request = api_kwargs.pop("_moa_prepared_request", None) —
     * a PRESENT null is None too: `if prepared_request is not None` skips
     * the prepared path for absent AND null (only non-null takes it). */
    const json_t *pr = json_obj_get(ak, "_moa_prepared_request");
    json_t *prep_copy = (pr && pr->type != JSON_NULL) ? json_copy(pr) : NULL;
    json_obj_del(ak, "_moa_prepared_request");
    if (prep_copy) {
        if (!ml3_is_dict(prep_copy)) {
            ml3_raise(exc, "TypeError", "_moa_prepared_request must be a dict");
            json_free(prep_copy); json_free(ak);
            return NULL;
        }
        ret = ml3_mcc_call_prepared_core(prep_copy, ak, io, state, exc);
        json_free(prep_copy); json_free(ak);
        return ret;
    }

    const json_t *pn = ml3_self_get(state, "preset_name", exc);
    if (*exc) goto cleanup;
    {
        json_t *a = json_object();
        ml3_arg_j(a, "preset_name", pn);
        res = ml3_call(io, "_resolve_preset_cached", a, exc);
        if (*exc) goto cleanup;
    }
    const json_t *ri[2];
    if (!ml3_unpack(res, 2, ri, exc)) goto cleanup;
    const json_t *preset = ri[0];
    const json_t *moa_raw = ri[1];
    {
        json_t *a = json_object();
        ml3_arg_j(a, "moa_raw", moa_raw);
        json_t *pm = ml3_call(io, "_moa_privacy_mode", a, exc);
        if (*exc) goto cleanup;
        json_set(state, "_privacy_mode", pm ? pm : json_null());
    }
    /* messages = list(api_kwargs.get("messages") or []) */
    {
        const json_t *mv = ml3_dict_get(ak, "messages", exc);
        if (*exc) goto cleanup;
        if (!ml3_truth(mv)) messages = json_array();
        else if (ml3_is_arr(mv)) messages = json_copy(mv);
        else {
            ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                          ml3_type_name(mv));
            goto cleanup;
        }
    }
    /* reference_models (enabled gate first, then the per-slot filter) */
    ref_models = json_array();
    {
        const json_t *pen = ml3_dict_get(preset, "enabled", exc);
        if (*exc) goto cleanup;
        bool enabled = pen ? ml3_truth(pen) : true;
        if (enabled) {
            const json_t *rm = ml3_dict_get(preset, "reference_models", exc);
            if (*exc) goto cleanup;
            if (ml3_truth(rm)) {
                if (!ml3_is_arr(rm)) {
                    ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                                  ml3_type_name(rm));
                    goto cleanup;
                }
                for (size_t i = 0; i < rm->c.count; i++) {
                    const json_t *slot = rm->c.items[i];
                    const json_t *en = ml3_dict_get(slot, "enabled", exc);
                    if (*exc) goto cleanup;
                    bool keep = en ? ml3_truth(en) : true;
                    if (keep) json_append(ref_models, json_copy(slot));
                }
            }
        }
    }
    /* aggregator = preset.get("aggregator") or {} */
    {
        const json_t *agv = ml3_dict_get(preset, "aggregator", exc);
        if (*exc) goto cleanup;
        aggregator = ml3_truth(agv) ? json_copy(agv) : json_object();
    }
    /* self.last_aggregator_slot = dict(aggregator) if aggregator else None */
    if (ml3_truth(aggregator)) {
        if (!ml3_is_dict(aggregator)) {
            ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                          ml3_type_name(aggregator));
            goto cleanup;
        }
        json_set(state, "last_aggregator_slot", json_copy(aggregator));
    } else {
        json_set(state, "last_aggregator_slot", json_null());
    }
    /* aggregator_temperature = _preset_temperature(preset, "aggregator_temperature") */
    {
        json_t *a = json_object();
        ml3_arg_j(a, "preset", preset);
        ml3_arg_s(a, "key", "aggregator_temperature");
        at = ml3_call(io, "_preset_temperature", a, exc);
        if (*exc) goto cleanup;
    }
    if (ml3_is_null(at)) {
        const json_t *tv = ml3_dict_get(ak, "temperature", exc);
        if (*exc) goto cleanup;
        if (tv && tv->type != JSON_NULL) {
            json_free(at);
            at = json_copy(tv);
        }
    }
    /* ref_messages = _reference_messages(messages) */
    {
        json_t *a = json_object();
        ml3_arg_j(a, "messages", messages);
        ref_messages = ml3_call(io, "_reference_messages", a, exc);
        if (*exc) goto cleanup;
    }
    /* cache_key = self._fanout_cache_key(preset, ref_messages, reference_models) */
    {
        json_t *a = json_object();
        ml3_arg_j(a, "preset", preset);
        ml3_arg_j(a, "ref_messages", ref_messages);
        ml3_arg_j(a, "reference_models", ref_models);
        cache_key = ml3_call(io, "_fanout_cache_key", a, exc);
        if (*exc) goto cleanup;
    }
    /* cache_hit = bool(cache_key == self._ref_cache_key and self._ref_cache_outputs) */
    const json_t *rck = ml3_self_get(state, "_ref_cache_key", exc);
    if (*exc) goto cleanup;
    const json_t *rco = ml3_self_get(state, "_ref_cache_outputs", exc);
    if (*exc) goto cleanup;
    bool cache_hit = ml3_deep_eq(cache_key, rck) && ml3_truth(rco);
    if (cache_hit) {
        if (!ml3_is_arr(rco)) {
            ml3_raise_fmt(exc, "TypeError", "'%s' object is not iterable",
                          ml3_type_name(rco));
            goto cleanup;
        }
        reference_outputs = json_copy(rco);
        json_set(state, "_pending_trace", json_null());
    } else {
        json_t *a = json_object();
        ml3_arg_j(a, "preset", preset);
        ml3_arg_j(a, "ref_messages", ref_messages);
        ml3_arg_j(a, "reference_models", ref_models);
        ml3_arg_j(a, "aggregator", aggregator);
        ml3_arg_j(a, "aggregator_temperature", at);
        ml3_arg_j(a, "cache_key", cache_key);
        reference_outputs = ml3_call(io, "_run_fanout", a, exc);
        if (*exc) goto cleanup;
    }
    /* agg_messages = [dict(m) for m in messages] */
    agg_messages = ml3_msg_copies(messages, exc);
    if (*exc) goto cleanup;
    /* str(preset.get("degraded_reference_policy") or "loud") — evaluated
     * BEFORE the stale kwarg's _tool_activity seam (arg order). */
    {
        const json_t *dp = ml3_dict_get(preset, "degraded_reference_policy", exc);
        if (*exc) goto cleanup;
        ds = ml3_pystr_or(dp, "loud");
    }
    bool stale = false;
    if (cache_hit) {
        json_t *a = json_object();
        ml3_arg_j(a, "messages", messages);
        json_t *tar = ml3_call(io, "_tool_activity_since_last_user", a, exc);
        if (*exc) goto cleanup;
        stale = ml3_truth(tar);
        json_free(tar);
    }
    /* guidance = self._build_guidance(..., stale=stale) */
    {
        json_t *a = json_object();
        ml3_arg_j(a, "reference_outputs", reference_outputs);
        ml3_arg_j(a, "aggregator", aggregator);
        ml3_arg_s(a, "degraded_reference_policy", ds ? ds : "loud");
        ml3_arg_b(a, "stale", stale);
        guidance = ml3_call(io, "_build_guidance", a, exc);
        if (*exc) goto cleanup;
    }
    if (ml3_truth(guidance)) {
        json_t *a = json_object();
        ml3_arg_j(a, "agg_messages", agg_messages);
        ml3_arg_j(a, "guidance", guidance);
        json_t *r = ml3_call(io, "_attach_reference_guidance", a, exc);
        if (*exc) goto cleanup;
        json_free(r);
    }
    prepared = json_object();
    json_set(prepared, "messages", json_copy(agg_messages));
    json_set(prepared, "guidance", guidance ? json_copy(guidance) : json_null());
    json_set(prepared, "aggregator", json_copy(aggregator));
    json_set(prepared, "aggregator_temperature", at ? json_copy(at) : json_null());
    /* if api_kwargs.pop("_moa_prepare_only", False): return prepared_request */
    {
        const json_t *po = json_obj_get(ak, "_moa_prepare_only");
        bool prepare_only = ml3_truth(po);
        json_obj_del(ak, "_moa_prepare_only");
        if (prepare_only) {
            ret = prepared;
            prepared = NULL;
            goto cleanup;
        }
    }
    ret = ml3_mcc_call_prepared_core(prepared, ak, io, state, exc);
    goto cleanup;

cleanup:
    json_free(res); json_free(messages); json_free(ref_models);
    json_free(aggregator); json_free(at); json_free(ref_messages);
    json_free(cache_key); json_free(reference_outputs);
    json_free(agg_messages); json_free(guidance); json_free(prepared);
    json_free(ak);
    free(ds);
    return ret;
}

static json_t *ml3_mcc_create(const json_t *in, const ml3_io_t *io,
                              json_t *state, json_t **exc) {
    const json_t *akj = json_obj_get(in, "api_kwargs");
    json_t *ak = ml3_is_dict(akj) ? json_copy(akj) : json_object();
    return ml3_mcc_create_core(ak, io, state, exc);
}

/* send(messages=agg_messages) args: the recorder projects the partial's
 * **kwargs and drops None-valued keys (ARGMAP), so the seam args carry only
 * the non-null kwargs. OWNED result. */
static json_t *ml3_send_args(const json_t *send_kwargs, const json_t *msgs) {
    json_t *a = json_copy(send_kwargs);
    json_set(a, "messages", msgs ? json_copy(msgs) : json_null());
    for (size_t i = 0; i < a->c.count; ) {
        if (a->c.items[i]->type == JSON_NULL) json_obj_del(a, a->c.keys[i]);
        else i++;
    }
    return a;
}

/* trace[key] = val — py3.14 item-assignment shapes (list takes the index
 * TypeError; other non-dicts the does-not-support message). Consumes val. */
static void ml3_trace_set(json_t *trace, const char *key, json_t *val,
                          json_t **exc) {
    if (ml3_is_arr(trace)) {
        ml3_raise(exc, "TypeError",
                  "list indices must be integers or slices, not str");
        json_free(val);
        return;
    }
    if (!ml3_is_dict(trace)) {
        ml3_raise_fmt(exc, "TypeError",
                      "'%s' object does not support item assignment",
                      ml3_type_name(trace));
        json_free(val);
        return;
    }
    json_set(trace, key, val);
}

/* ── 10. MoAChatCompletions._call_prepared_aggregator ───────────────────────
 *   aggregator = prepared["aggregator"]
 *   if aggregator.get("provider") == "moa":
 *       raise RuntimeError("MoA aggregator cannot be another MoA preset")
 *   agg_runtime = _slot_runtime(aggregator)
 *   agg_messages, tools = self._plan_aggregator_cache(prepared["messages"],
 *       api_kwargs.get("tools"), prepared.get("guidance"), agg_runtime)
 *   from agent.prompt_cache_clock import attempt_facts
 *   self.last_aggregator_route = {k: agg_runtime.get(k)
 *       for k in ("provider", "model", "base_url", "api_mode")}
 *   trace = self._pending_trace
 *   if trace is not None: trace["aggregator_label"] = _slot_label(aggregator)
 *   stream = bool(api_kwargs.get("stream"))
 *   stream_kwargs = {}
 *   if stream:
 *       stream_kwargs = {"stream": True,
 *           "stream_options": api_kwargs.get("stream_options")
 *               or {"include_usage": True}}
 *       if api_kwargs.get("timeout") is not None:
 *           stream_kwargs["timeout"] = api_kwargs["timeout"]
 *   agg_extra_body = _merge_slot_extra_body(agg_runtime.pop("extra_body", None),
 *       api_kwargs.get("extra_body"))
 *   destination = destination_key(agg_runtime)
 *   remembered = getattr(self, "_merge_same_role_destinations", None)
 *   if remembered is None:
 *       remembered = self._merge_same_role_destinations = set()
 *   merged = destination in remembered
 *   if merged: agg_messages = merge_same_role_messages(agg_messages)
 *   send = functools.partial(call_llm, task="moa_aggregator",
 *       temperature=prepared["aggregator_temperature"],
 *       max_tokens=api_kwargs.get("max_tokens"), tools=tools,
 *       extra_body=agg_extra_body,
 *       reasoning_config=_aggregator_reasoning_config(aggregator),
 *       **stream_kwargs, **agg_runtime)
 *   self.last_aggregator_attempt = attempt_facts(
 *       {"messages": agg_messages, "tools": tools, "extra_body": agg_extra_body})
 *   try: agg_response = send(messages=agg_messages)
 *   except Exception as exc:
 *       retry_messages = None if merged else merge_same_role_messages(agg_messages)
 *       if retry_messages is None or retry_messages is agg_messages
 *           or not is_role_alternation_rejection(exc, agg_runtime): raise
 *       remembered.add(destination)
 *       logger.warning(..., _slot_label(aggregator), exc)   (args evaluate!)
 *       agg_messages = retry_messages
 *       self.last_aggregator_attempt = attempt_facts({...})
 *       agg_response = send(messages=agg_messages)
 *   if trace is not None:
 *       trace["aggregator_input_messages"] = (
 *           _redact_trace_messages([dict(m) for m in agg_messages])
 *           if getattr(self, "_privacy_mode", "") else agg_messages)
 *       trace["aggregator_streamed"] = stream
 *       output = None
 *       if not stream:
 *           with contextlib.suppress(Exception): output = _extract_text(agg_response)
 *       trace["aggregator_output"] = output
 *   if stream and hasattr(agg_response, "choices"):
 *       return iter((_completed_response_as_stream_chunk(agg_response),))
 *   return agg_response
 * Borrows prepared + ak (read-only). The merge seam's {"__echo__":"messages"}
 * ret models the live callee returning its input object itself (identity).
 */
static json_t *ml3_mcc_call_prepared_core(const json_t *prepared,
                                          const json_t *ak,
                                          const ml3_io_t *io, json_t *state,
                                          json_t **exc) {
    json_t *agg_runtime = NULL, *agg_messages = NULL, *tools = NULL;
    json_t *stream_kwargs = NULL, *send_kwargs = NULL;
    json_t *agg_extra_body = NULL, *destination = NULL;
    json_t *agg_response = NULL, *ret = NULL;
    json_t *plan = NULL, *reasoning = NULL;   /* reasoning: init HERE — early
                                               * goto cleanup jumps over its
                                               * old mid-function initializer */

    const json_t *aggregator = ml3_subscript(prepared, "aggregator", exc);
    if (*exc) return NULL;
    {
        const json_t *prov = ml3_dict_get(aggregator, "provider", exc);
        if (*exc) return NULL;
        if (ml3_is_str(prov) && strcmp(prov->str_val, "moa") == 0) {
            ml3_raise(exc, "RuntimeError",
                      "MoA aggregator cannot be another MoA preset");
            return NULL;
        }
    }
    {
        json_t *a = json_object();
        ml3_arg_j(a, "slot", aggregator);
        agg_runtime = ml3_call(io, "_slot_runtime", a, exc);
        if (*exc) return NULL;
    }
    {
        const json_t *pm = ml3_subscript(prepared, "messages", exc);
        if (*exc) goto cleanup;
        const json_t *tl = ml3_dict_get(ak, "tools", exc);
        if (*exc) goto cleanup;
        const json_t *gd = ml3_dict_get(prepared, "guidance", exc);
        if (*exc) goto cleanup;
        json_t *a = json_object();
        ml3_arg_j(a, "agg_messages", pm);
        ml3_arg_j(a, "tools", tl);
        ml3_arg_j(a, "guidance", gd);
        ml3_arg_j(a, "agg_runtime", agg_runtime);
        plan = ml3_call(io, "_plan_aggregator_cache", a, exc);
        if (*exc) goto cleanup;
    }
    {
        const json_t *pi[2];
        if (!ml3_unpack(plan, 2, pi, exc)) goto cleanup;
        agg_messages = pi[0] ? json_copy(pi[0]) : json_null();
        tools = pi[1] ? json_copy(pi[1]) : json_null();
        json_free(plan); plan = NULL;
    }
    /* self.last_aggregator_route = {k: agg_runtime.get(k) for k in (...)} */
    {
        static const char *RK[] = {"provider", "model", "base_url", "api_mode"};
        json_t *route = json_object();
        for (size_t i = 0; i < 4; i++) {
            const json_t *v = ml3_dict_get(agg_runtime, RK[i], exc);
            if (*exc) { json_free(route); goto cleanup; }
            json_set(route, RK[i], v ? json_copy(v) : json_null());
        }
        json_set(state, "last_aggregator_route", route);
    }
    const json_t *trace = ml3_self_get(state, "_pending_trace", exc);
    if (*exc) goto cleanup;
    if (!ml3_is_null(trace)) {
        json_t *a = json_object();
        ml3_arg_j(a, "slot", aggregator);
        json_t *lbl = ml3_call(io, "_slot_label", a, exc);
        if (*exc) goto cleanup;
        ml3_trace_set((json_t *)trace, "aggregator_label",
                      lbl ? lbl : json_null(), exc);
        if (*exc) goto cleanup;
    }
    /* stream + stream_kwargs */
    const json_t *sv = ml3_dict_get(ak, "stream", exc);
    if (*exc) goto cleanup;
    bool stream = ml3_truth(sv);
    stream_kwargs = json_object();
    if (stream) {
        json_set(stream_kwargs, "stream", json_bool(true));
        const json_t *so = ml3_dict_get(ak, "stream_options", exc);
        if (*exc) goto cleanup;
        if (ml3_truth(so)) {
            json_set(stream_kwargs, "stream_options", json_copy(so));
        } else {
            json_t *def = json_object();
            json_set(def, "include_usage", json_bool(true));
            json_set(stream_kwargs, "stream_options", def);
        }
        const json_t *to = ml3_dict_get(ak, "timeout", exc);
        if (*exc) goto cleanup;
        if (to && to->type != JSON_NULL)
            json_set(stream_kwargs, "timeout", json_copy(to));
    }
    /* agg_extra_body = _merge_slot_extra_body(agg_runtime.pop("extra_body",
     *     None), api_kwargs.get("extra_body")) */
    {
        json_t *eb = ml3_dict_pop(agg_runtime, "extra_body", exc);
        if (*exc) goto cleanup;
        const json_t *ceb = ml3_dict_get(ak, "extra_body", exc);
        if (*exc) { json_free(eb); goto cleanup; }
        json_t *a = json_object();
        ml3_arg_j(a, "slot_extra_body", eb);
        ml3_arg_j(a, "caller_extra_body", ceb);
        json_free(eb);
        agg_extra_body = ml3_call(io, "_merge_slot_extra_body", a, exc);
        if (*exc) goto cleanup;
    }
    /* destination = destination_key(agg_runtime) */
    {
        json_t *a = json_object();
        ml3_arg_j(a, "runtime", agg_runtime);
        destination = ml3_call(io, "destination_key", a, exc);
        if (*exc) goto cleanup;
    }
    /* remembered = getattr(self, "_merge_same_role_destinations", None) */
    json_t *remembered = (json_t *)ml3_self_get_def(state, "_merge_same_role_destinations");
    if (ml3_is_null(remembered)) {
        remembered = json_array();
        json_set(state, "_merge_same_role_destinations", remembered);
    }
    /* merged = destination in remembered — destination_key materializes a
     * list result to a TUPLE (shallow) live, so a flat array is hashable;
     * a dict result or a tuple carrying a list/dict element raises the
     * py3.14 set-element TypeError (outer name = the materialized type). */
    {
        const json_t *bad = NULL;
        const char *outer = NULL;
        if (ml3_is_dict(destination)) {
            bad = destination; outer = "dict";
        } else if (ml3_is_arr(destination)) {
            outer = "tuple";
            for (size_t i = 0; i < destination->c.count; i++) {
                const json_t *el = destination->c.items[i];
                if (ml3_is_arr(el) || ml3_is_dict(el)) { bad = el; break; }
            }
        }
        if (bad) {
            ml3_raise_fmt(exc, "TypeError",
                          "cannot use '%s' as a set element (unhashable type: '%s')",
                          outer, ml3_type_name(bad));
            goto cleanup;
        }
    }
    bool merged = ml3_arr_contains(remembered, destination);
    if (merged) {
        json_t *a = json_object();
        ml3_arg_j(a, "messages", agg_messages);
        json_t *mm = ml3_call(io, "merge_same_role_messages", a, exc);
        if (*exc) goto cleanup;
        if (mm && ml3_is_obj(mm) && json_obj_get(mm, "__echo__")) {
            json_free(mm);   /* identity: agg_messages unchanged */
        } else {
            json_free(agg_messages);
            agg_messages = mm ? mm : json_null();
        }
    }
    /* send = functools.partial(call_llm, ..., **stream_kwargs, **agg_runtime) */
    const json_t *at = ml3_subscript(prepared, "aggregator_temperature", exc);
    if (*exc) goto cleanup;
    const json_t *mt = ml3_dict_get(ak, "max_tokens", exc);
    if (*exc) goto cleanup;
    {
        json_t *a = json_object();
        ml3_arg_j(a, "aggregator", aggregator);
        reasoning = ml3_call(io, "_aggregator_reasoning_config", a, exc);
        if (*exc) goto cleanup;
    }
    send_kwargs = json_object();
    json_set(send_kwargs, "task", json_string("moa_aggregator"));
    json_set(send_kwargs, "temperature", at ? json_copy(at) : json_null());
    json_set(send_kwargs, "max_tokens", mt ? json_copy(mt) : json_null());
    json_set(send_kwargs, "tools", json_copy(tools));
    json_set(send_kwargs, "extra_body",
             agg_extra_body ? json_copy(agg_extra_body) : json_null());
    json_set(send_kwargs, "reasoning_config", reasoning ? reasoning : json_null());
    reasoning = NULL;   /* owned by send_kwargs */
    /* **stream_kwargs then **agg_runtime — duplicate keys raise the live
     * functools.partial TypeError (first duplicate in iteration order). */
    for (size_t i = 0; i < stream_kwargs->c.count; i++) {
        const char *k = stream_kwargs->c.keys[i];
        if (json_obj_get(send_kwargs, k)) {
            ml3_raise_fmt(exc, "TypeError",
                          "functools.partial() got multiple values for "
                          "keyword argument '%s'", k);
            goto cleanup;
        }
        json_set(send_kwargs, k, json_copy(stream_kwargs->c.items[i]));
    }
    for (size_t i = 0; i < agg_runtime->c.count; i++) {
        const char *k = agg_runtime->c.keys[i];
        if (json_obj_get(send_kwargs, k)) {
            ml3_raise_fmt(exc, "TypeError",
                          "functools.partial() got multiple values for "
                          "keyword argument '%s'", k);
            goto cleanup;
        }
        json_set(send_kwargs, k, json_copy(agg_runtime->c.items[i]));
    }
    /* self.last_aggregator_attempt = attempt_facts({...}) */
    {
        json_t *p = json_object();
        json_set(p, "messages", json_copy(agg_messages));
        json_set(p, "tools", json_copy(tools));
        json_set(p, "extra_body",
                 agg_extra_body ? json_copy(agg_extra_body) : json_null());
        json_t *a = json_object();
        ml3_arg_j(a, "payload", p);
        json_free(p);
        json_t *attempt = ml3_call(io, "attempt_facts", a, exc);
        if (*exc) goto cleanup;
        json_set(state, "last_aggregator_attempt", attempt ? attempt : json_null());
    }
    /* try: agg_response = send(messages=agg_messages) */
    agg_response = ml3_call(io, "call_llm",
                            ml3_send_args(send_kwargs, agg_messages), exc);
    if (*exc) {
        /* except Exception as exc: BaseException-only kinds pass through. */
        if (!ml3_exc_is_exception(json_get_str(*exc, "__raise__", "")))
            goto cleanup;
        json_t *orig = *exc; *exc = NULL;
        json_t *retry = NULL;
        bool echo_same = false;
        if (!merged) {
            json_t *a = json_object();
            ml3_arg_j(a, "messages", agg_messages);
            retry = ml3_call(io, "merge_same_role_messages", a, exc);
            if (*exc) { json_free(orig); goto cleanup; }
            if (retry && ml3_is_obj(retry) && json_obj_get(retry, "__echo__"))
                echo_same = true;
        }
        bool rethrow = (!retry || echo_same);
        if (!rethrow) {
            json_t *a = json_object();
            ml3_arg_s(a, "exc", json_get_str(orig, "msg", ""));
            ml3_arg_j(a, "runtime", agg_runtime);
            json_t *rej = ml3_call(io, "is_role_alternation_rejection", a, exc);
            if (*exc) { json_free(orig); json_free(retry); goto cleanup; }
            rethrow = !ml3_truth(rej);
            json_free(rej);
        }
        if (rethrow) {
            *exc = orig;
            json_free(retry);
            goto cleanup;
        }
        json_free(orig);
        /* remembered.add(destination) */
        ml3_set_add(remembered, destination);
        /* logger.warning(..., _slot_label(aggregator), exc) — silent, but
         * the _slot_label argument seam still fires. */
        {
            json_t *a = json_object();
            ml3_arg_j(a, "slot", aggregator);
            json_t *lbl = ml3_call(io, "_slot_label", a, exc);
            if (*exc) { json_free(retry); goto cleanup; }
            json_free(lbl);
        }
        json_free(agg_messages);
        agg_messages = retry;   /* ownership transfer */
        {
            json_t *p = json_object();
            json_set(p, "messages", json_copy(agg_messages));
            json_set(p, "tools", json_copy(tools));
            json_set(p, "extra_body",
                     agg_extra_body ? json_copy(agg_extra_body) : json_null());
            json_t *a = json_object();
            ml3_arg_j(a, "payload", p);
            json_free(p);
            json_t *attempt = ml3_call(io, "attempt_facts", a, exc);
            if (*exc) goto cleanup;
            json_set(state, "last_aggregator_attempt",
                     attempt ? attempt : json_null());
        }
        agg_response = ml3_call(io, "call_llm",
                                ml3_send_args(send_kwargs, agg_messages), exc);
        if (*exc) goto cleanup;
    }
    /* if trace is not None: trace the exact aggregator input as sent */
    if (!ml3_is_null(trace)) {
        const json_t *pmode = ml3_self_get_def(state, "_privacy_mode");
        json_t *aim = NULL;
        if (ml3_truth(pmode)) {
            json_t *copies = ml3_msg_copies(agg_messages, exc);
            if (*exc) goto cleanup;
            json_t *a = json_object();
            ml3_arg_j(a, "messages", copies);
            json_free(copies);
            aim = ml3_call(io, "_redact_trace_messages", a, exc);
            if (*exc) goto cleanup;
            if (!aim) aim = json_null();
        } else {
            aim = json_copy(agg_messages);
        }
        ml3_trace_set((json_t *)trace, "aggregator_input_messages", aim, exc);
        if (*exc) goto cleanup;
        json_set((json_t *)trace, "aggregator_streamed", json_bool(stream));
        json_t *output = json_null();
        if (!stream) {
            /* with contextlib.suppress(Exception): output = _extract_text(...) */
            json_t *e3 = NULL;
            json_t *a = json_object();
            ml3_arg_j(a, "response", agg_response);
            json_t *et = ml3_call(io, "_extract_text", a, &e3);
            if (e3) {
                if (!ml3_swallow(&e3)) {
                    json_free(output);
                    *exc = e3;
                    goto cleanup;
                }
            } else {
                json_free(output);
                output = et ? et : json_null();
            }
        }
        json_set((json_t *)trace, "aggregator_output", output);
    }
    /* if stream and hasattr(agg_response, "choices"): one-chunk iterator */
    if (stream && ml3_hasattr(agg_response, "choices")) {
        json_t *a = json_object();
        ml3_arg_j(a, "response", agg_response);
        json_t *chunk = ml3_call(io, "_completed_response_as_stream_chunk", a, exc);
        if (*exc) goto cleanup;
        json_t *it = json_object();
        json_t *items = json_array();
        json_append(items, chunk ? chunk : json_null());
        json_set(it, "__iter__", items);
        ret = it;
        goto cleanup;
    }
    ret = agg_response ? agg_response : json_null();
    agg_response = NULL;   /* ownership transfers to ret */
    goto cleanup;

cleanup:
    json_free(agg_runtime); json_free(agg_messages); json_free(tools);
    json_free(stream_kwargs); json_free(send_kwargs);
    json_free(agg_extra_body); json_free(destination);
    json_free(agg_response); json_free(plan); json_free(reasoning);
    return ret;
}

static json_t *ml3_mcc_call_prepared(const json_t *in, const ml3_io_t *io,
                                     json_t *state, json_t **exc) {
    const json_t *prepared = json_obj_get(in, "prepared");
    const json_t *akj = json_obj_get(in, "api_kwargs");
    json_t *empty = NULL;
    const json_t *ak = akj;
    if (ml3_is_null(ak)) { empty = json_object(); ak = empty; }
    json_t *r = ml3_mcc_call_prepared_core(prepared, ak, io, state, exc);
    json_free(empty);
    return r;
}

/* ── 11-15. MoAClient delegation surface ─────────────────────────────────── */

/* Navigate self.chat.completions (direct attribute reads; the live client
 * double is a SimpleNamespace). Returns the borrowed completions value
 * (possibly a null node / scalar) or NULL with *exc set. */
static const json_t *ml3_client_completions(const json_t *state, json_t **exc) {
    const json_t *chat = json_obj_get(state, "chat");
    if (!chat) { ml3_ns_attr_err(exc, "chat"); return NULL; }
    if (ml3_is_null(chat)) {
        ml3_raise_fmt(exc, "AttributeError",
                      "'NoneType' object has no attribute 'completions'");
        return NULL;
    }
    if (!ml3_is_obj(chat)) { ml3_attr_err(exc, chat, "completions"); return NULL; }
    const json_t *comp = json_obj_get(chat, "completions");
    if (!comp) { ml3_ns_attr_err(exc, "completions"); return NULL; }
    return comp;
}

/* The delegated method attribute on the completions carrier: a facade
 * window (plain object) carries the bound ported method; any other carrier
 * raises the method-named AttributeError. */
static json_t *ml3_client_method_carrier(const json_t *comp, const char *name,
                                         json_t **exc) {
    if (ml3_is_null(comp) || !ml3_is_obj(comp)) {
        ml3_attr_err(exc, comp, name);
        return NULL;
    }
    return (json_t *)comp;
}

/* ── 11. MoAClient.__init__ ─────────────────────────────────────────────────
 *   self.chat = type("_MoAChat", (), {})()
 *   self.chat.completions = MoAChatCompletions(preset_name,
 *       reference_callback=reference_callback, agent=agent)
 * Fresh client window; composes the ported MoAChatCompletions.__init__ REAL.
 */
static json_t *ml3_client_init(const json_t *in, const ml3_io_t *io,
                               json_t *state, json_t **exc) {
    json_t *comp = json_object();
    json_t *r = ml3_mcc_init(in, io, comp, exc);
    if (*exc) { json_free(comp); return NULL; }
    json_free(r);
    json_t *chat = json_object();
    json_set(chat, "completions", comp);
    json_set(state, "chat", chat);
    return json_null();
}

/* ── 12. MoAClient.consume_reference_usage ──────────────────────────────────
 *   return self.chat.completions.consume_reference_usage()
 */
static json_t *ml3_client_consume_reference_usage(const json_t *in,
                                                  const ml3_io_t *io,
                                                  json_t *state, json_t **exc) {
    const json_t *comp = ml3_client_completions(state, exc);
    if (*exc) return NULL;
    json_t *carrier = ml3_client_method_carrier(comp, "consume_reference_usage", exc);
    if (*exc) return NULL;
    return ml3_mcc_consume_reference_usage(in, io, carrier, exc);
}

/* ── 13. MoAClient.last_aggregator_slot (property) ──────────────────────────
 *   return getattr(self.chat.completions, "last_aggregator_slot", None)
 */
static json_t *ml3_client_last_aggregator_slot(const json_t *in,
                                               const ml3_io_t *io,
                                               json_t *state, json_t **exc) {
    (void)in; (void)io;
    const json_t *comp = ml3_client_completions(state, exc);
    if (*exc) return NULL;
    /* getattr with default: only an attribute window carries the field. */
    if (ml3_is_null(comp) || !ml3_is_obj(comp)) return json_null();
    const json_t *v = json_obj_get(comp, "last_aggregator_slot");
    return v ? json_copy(v) : json_null();
}

/* ── 14. MoAClient.consume_and_save_trace ───────────────────────────────────
 *   return self.chat.completions.consume_and_save_trace(session_id,
 *       aggregator_output_fallback=aggregator_output_fallback)
 */
static json_t *ml3_client_consume_and_save_trace(const json_t *in,
                                                 const ml3_io_t *io,
                                                 json_t *state, json_t **exc) {
    const json_t *comp = ml3_client_completions(state, exc);
    if (*exc) return NULL;
    json_t *carrier = ml3_client_method_carrier(comp, "consume_and_save_trace", exc);
    if (*exc) return NULL;
    return ml3_mcc_consume_and_save_trace(in, io, carrier, exc);
}

/* ── 15. MoAClient.last_reference_metrics ───────────────────────────────────
 *   return self.chat.completions.last_reference_metrics()
 */
static json_t *ml3_client_last_reference_metrics(const json_t *in,
                                                 const ml3_io_t *io,
                                                 json_t *state, json_t **exc) {
    const json_t *comp = ml3_client_completions(state, exc);
    if (*exc) return NULL;
    json_t *carrier = ml3_client_method_carrier(comp, "last_reference_metrics", exc);
    if (*exc) return NULL;
    return ml3_mcc_last_reference_metrics(in, io, carrier, exc);
}
