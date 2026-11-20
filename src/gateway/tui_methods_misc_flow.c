/*
 * tui_methods_misc_flow.c — FLOW + leaf port of the TUI slash-exec misc
 * method surface (hermes-agent).
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_misc.py, which does
 * NOT exist in the quarry. The TUI's miscellaneous slash.exec helpers (live
 * command output + side-effect mirroring) live in tui_gateway/methods_slash.py
 * — fully UNCLAIMED at port time (grep PoP by Python name). That is the REAL
 * defining module; every PoP below credits it (the scanner only credits the
 * defining module). The bodies are rebound onto tui_gateway/server.py's
 * globals at install time (method_ctx.bind_module), so server globals
 * (_emit, _session_info, _sessions helpers, ...) enter through the seam.
 *
 * Ports the DECISION SEQUENCES — branch order, seam call order, exception
 * paths and the agent.service_tier state transition — of all 25 named defs:
 *   _format_live_review_output, _format_live_refine_output, _usage_route,
 *   _usage_on_nous_route, _usage_nous_fields, _usage_account_blocks,
 *   _format_live_usage_output, _format_live_usage_tokens,
 *   _live_session_messages, _format_live_history_output,
 *   _format_live_context_output, _format_live_tools_output,
 *   _format_live_help_output, _format_live_model_output,
 *   _format_live_status_output, _live_slash_command_output,
 *   _compress_live_with_feedback, _mirror_approvals, _mirror_personality,
 *   _mirror_fast, _mirror_reload_mcp, _mirror_stop, _mirror_autoprune,
 *   _compute_host_slash, _mirror_slash_side_effects.
 * (register() is the bind_module installer — not a decision flow.)
 *
 * Every live external call enters through ONE universal seam:
 * tmm_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Ported siblings are SEAMED inside flows (each is exercised by its
 * own direct cases, where the real body runs on both sides).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmm_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   format_live_review_output   _format_live_review_output(sid, session, arg)
 *   format_live_refine_output   _format_live_refine_output(sid, session, arg)
 *   format_live_usage_output    _format_live_usage_output(sid, session, arg)
 *   format_live_usage_tokens    _format_live_usage_tokens(session)
 *   format_live_history_output  _format_live_history_output(sid, session, arg)
 *   format_live_context_output  _format_live_context_output(sid, session, arg)
 *   format_live_tools_output    _format_live_tools_output(sid, session, arg)
 *   format_live_help_output     _format_live_help_output(sid, session, arg)
 *   format_live_model_output    _format_live_model_output(session)
 *   format_live_status_output   _format_live_status_output(sid, session, arg)
 *   usage_route                 _usage_route(session)
 *   usage_on_nous_route         _usage_on_nous_route(route)
 *   usage_account_blocks        _usage_account_blocks(session)
 *   live_session_messages       _live_session_messages(session)
 *   compute_host_slash          _compute_host_slash(sid, session, name,
 *                               command) -> [status, text]
 *   compress_live_with_feedback _compress_live_with_feedback(sid, session,
 *                               agent, arg, snapshot_kwargs=)
 *   mirror_approvals            _mirror_approvals(sid, session, agent, arg)
 *   mirror_personality          _mirror_personality(sid, session, agent, arg)
 *   mirror_fast                 _mirror_fast(sid, session, agent, arg)
 *   mirror_reload_mcp           _mirror_reload_mcp(sid, session, agent, arg)
 *   mirror_stop                 _mirror_stop(sid, session, agent, arg)
 *   mirror_autoprune            _mirror_autoprune(sid, session, agent, arg)
 *   mirror_slash_side_effects   _mirror_slash_side_effects(sid, session,
 *                               command)
 *   session_uses_compute_host   _session_uses_compute_host(session)
 *   set_session_context         _set_session_context(session_key,
 *                               ui_session_id= | cwd=) -> tokens
 *   clear_session_context       _clear_session_context(tokens)
 *   runtime_record_set          _current_runtime_session_record.set(session)
 *                               -> token
 *   runtime_record_reset        _current_runtime_session_record.reset(token)
 *   profile_runtime_scope       _session_profile_runtime_scope(session) CM
 *                               [enter/exit silent; script = yielded value]
 *   session_db                  _session_db(session) CM [enter yields the
 *                               scripted db marker; exit silent]
 *   persistence_for             _persistence_for(db) CM [enter yields the
 *                               scripted persistence dict|null; exit silent]
 *   persistence.read_conversation  persistence.messages.read_conversation(
 *                               ref, ConversationRequest(...)) -> message
 *                               doubles; args carry include_lineage /
 *                               include_row_ids projections
 *   message.as_openai_message   each message double's .as_openai_message()
 *                               [per-call queue supplies the openai dicts]
 *   deferred_build_agent_kwargs _deferred_build_agent_kwargs(session, None)
 *   resolve_agent_model_runtime _resolve_agent_model_runtime(model_override,
 *                               provider_override) -> [name, route]; [1] read
 *   is_nous_inference_route     agent.billing_links.is_nous_inference_route(
 *                               provider or "", base_url or "")
 *   nous_credits_usage_fields   agent.account_usage_nous.
 *                               nous_credits_usage_fields(agent, nous_route=)
 *   account_limit_lines         agent.account_usage.account_limit_lines(
 *                               provider, base_url=, api_key=)
 *   nous_usage_lines            agent.account_usage_nous.nous_usage_lines(
 *                               nous_route=, credits_state=)
 *   session_usage_snapshot      _session_usage_snapshot(session)
 *   metadata_mirror             _metadata_mirror(session)
 *   history_to_messages         _history_to_messages(history, profile_home=)
 *   session_cwd                 _session_cwd(session)
 *   compute_session_context_breakdown  agent.context_breakdown.
 *                               compute_session_context_breakdown(agent, msgs)
 *   compute_context_details     agent.context_breakdown.
 *                               compute_context_details(agent)
 *   render_context_breakdown_lines  agent.context_breakdown.
 *                               render_context_breakdown_lines(payload,
 *                               details=, grid=False)
 *   context_file_sources_for_agent  agent.context_file_sources.
 *                               context_file_sources_for_agent(agent)
 *   render_context_file_lines   agent.context_file_sources.
 *                               render_context_file_lines(sources)
 *   session_info                _session_info(agent, session)
 *   methods.session_status      _methods["session.status"]("status",
 *                               {"session_id": sid})
 *   emit                        _emit(event, sid, payload)
 *   broadcast_session_info      broadcast_session_info()
 *   load_cfg                    _load_cfg()
 *   validate_personality        _validate_personality(arg, cfg) -> [pname,
 *                               new_prompt]
 *   persist_personality         hermes_cli.personality.persist_personality(
 *                               pname)
 *   apply_personality_to_session  _apply_personality_to_session(sid, session,
 *                               new_prompt, pname)
 *   manual_compress_turn        _manual_compress_turn(sid, session) CM
 *                               [enter/exit silent]
 *   compress_session_history    _compress_session_history(session, arg
 *                               [, **snapshot])
 *   sync_session_key_after_compress  _sync_session_key_after_compress(sid,
 *                               session)
 *   apply_model_switch          _apply_model_switch(sid, session, arg)
 *   busy_message                busy_message(name, bool(_manual_compress_active))
 *   send_compute_host_control   _send_compute_host_control(sid, route_name=,
 *                               command=, wait=True)
 *   send_claimed_compute_host_control  _send_claimed_compute_host_control(
 *                               sid, session, route_name=, command=,
 *                               compress=)
 *   apply_compute_host_metadata_mirror  _apply_compute_host_metadata_mirror(
 *                               session, ack)
 *   process_registry.kill_all   tools.process_registry.process_registry.
 *                               kill_all(source="slash.stop")
 *   prune_advisor.answered      hermes_cli.prune_advisor.answered(Path(home)
 *                               if home else None) -> POSIX home projection
 *   start_review                agent.review_engine.start_review(agent,
 *                               snapshot, arg or "")
 *   format_dispatch_note        agent.review_engine.format_dispatch_note(
 *                               result, arg or "")
 *   parse_compress_args         agent.conversation_compression_manual.
 *                               parse_compress_args(arg) -> request double
 *   compress_now                agent.conversation_compression_manual.
 *                               compress_now(agent, preview_messages, request)
 *   render_compress_result      agent.conversation_compression_manual.
 *                               render_compress_result(result)
 *   estimate_request_tokens_rough  agent.model_metadata.
 *                               estimate_request_tokens_rough(messages,
 *                               system_prompt=, tools=) [no call when the
 *                               messages list is empty — estimate() returns 0]
 *   describe_compression_lock_skip  agent.manual_compression_feedback.
 *                               describe_compression_lock_skip(e.holder)
 *   summarize_manual_compression  agent.manual_compression_feedback.
 *                               summarize_manual_compression(before, after,
 *                               before_tokens, after_tokens,
 *                               compression_state=)
 *   finalize_context_engine_notification  agent.conversation_compression.
 *                               finalize_context_engine_compression_notification(
 *                               agent, committed=)
 *   logger.debug                logger.debug(msg, exc_info=True)
 *   commands_by_category        hermes_cli.commands.COMMANDS_BY_CATEGORY
 *                               [module __getattr__ fires at import time]
 *   agent.spawn_background_review  agent._spawn_background_review(
 *                               messages_snapshot=, review_memory=True,
 *                               review_skills=, focus=, explicit=True)
 *   agent.reload_mcp_tools      agent.reload_mcp_tools() [hasattr-gated]
 *
 * Representation contract (shared with tests/oracle_tui_methods_misc_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The FULL builtin hierarchy plus
 *   queue.Empty(Exception), CompressionLockHeld(Exception),
 *   CompressionBusy(Exception), SessionNotOwned(RuntimeError) and
 *   AuthError(RuntimeError) ride the tmm_exc_parents table.
 * - CompressionLockHeld carries "holder" in its spec; str(exc) is
 *   "Compression lock held: {holder or 'unknown'}" (its live __init__).
 * - session is a fixture dict; its "agent" value (null or dict) materializes
 *   to a SimpleNamespace double live. Agent OBJECTS are always truthy (even
 *   {}); C tests presence (non-null), not container truthiness, for agent.
 *   Seam args project session via proj_session: agent -> "agent" marker.
 *   history_lock: a truthy fixture value (marker "lock") is a silent CM
 *   double live; `with session["history_lock"]` on a MISSING key raises
 *   KeyError('history_lock'). Lock enter/exit fire no events.
 * - request (parse_compress_args) and _credits_state dicts materialize to
 *   SimpleNamespaces live; a missing attribute on them raises the
 *   module-qualified 'types.SimpleNamespace' AttributeError.
 * - Doubles passed through seams (route/build/usage/mirror/fb/ack/response/
 *   info/payload/details/sources/result/fb) stay fixture dicts; .get sites
 *   model the AttributeError on non-dict carriers.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings (fixture paths are POSIX, no drive letters/UNC).
 * - str()/slicing/title()/lower() fold the ASCII fixture corpus only.
 *   Output CONSTANTS carry the live non-ASCII glyphs (em dash, U+2697, 40x
 *   U+2500 separator) verbatim as UTF-8 literals.
 * - State: ctx.state mirrors agent.service_tier (set by _mirror_fast whenever
 *   the agent is present; null when the tier arg is not in _FAST_TIERS).
 *   Other functions leave state alone.
 * - Fixture domains: usage/route/build/fb/ack/response/info dicts; tool names
 *   and roles str/int; numbers integral; no NaN/Infinity; no integral floats
 *   in str()-computed or type-name positions; history_lock present => truthy
 *   marker; valid_tool_names a list; reload_mcp_tools present => true.
 *
 * Ownership: tmm_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node — borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
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

/* ── universal seam context ── */
typedef struct tmm_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmm_ctx_t;

/* ── basic helpers ── */
static bool tmm_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmm_raise_kind(const json_t *v) {
    return tmm_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmm_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmm_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmm_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmm_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static char *tmm_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain; they serialize as JSON, which no fixture exercises). */
static char *tmm_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmm_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmm_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmm_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tmm_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmm_json_eq(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return false;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type != b->type) return false;
    if (a->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tmm_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmm_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* ── exception hierarchy (FULL builtin tree + the quarry customs) ──
 * An unknown scripted kind defaults to Exception — exactly the oracle
 * make_exc fallback type(kind, (Exception,)). BaseException-only kinds never
 * reach Exception. SessionNotOwned is a RuntimeError (session_lifecycle.py);
 * CompressionLockHeld / CompressionBusy are plain Exceptions
 * (tui_gateway/session_compression.py); AuthError is a RuntimeError
 * (hermes_cli/auth_constants.py). */
typedef struct tmm_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} tmm_exc_parent_t;

static const tmm_exc_parent_t TMM_EXC_PARENTS[] = {
    {"BaseException", {NULL}},
    {"Exception", {"BaseException", NULL}},
    {"SystemExit", {"BaseException", NULL}},
    {"KeyboardInterrupt", {"BaseException", NULL}},
    {"GeneratorExit", {"BaseException", NULL}},
    {"CancelledError", {"BaseException", NULL}},
    {"StopIteration", {"Exception", NULL}},
    {"StopAsyncIteration", {"Exception", NULL}},
    {"ArithmeticError", {"Exception", NULL}},
    {"FloatingPointError", {"ArithmeticError", NULL}},
    {"OverflowError", {"ArithmeticError", NULL}},
    {"ZeroDivisionError", {"ArithmeticError", NULL}},
    {"AssertionError", {"Exception", NULL}},
    {"AttributeError", {"Exception", NULL}},
    {"BufferError", {"Exception", NULL}},
    {"EOFError", {"Exception", NULL}},
    {"ImportError", {"Exception", NULL}},
    {"ModuleNotFoundError", {"ImportError", NULL}},
    {"LookupError", {"Exception", NULL}},
    {"IndexError", {"LookupError", NULL}},
    {"KeyError", {"LookupError", NULL}},
    {"MemoryError", {"Exception", NULL}},
    {"NameError", {"Exception", NULL}},
    {"UnboundLocalError", {"NameError", NULL}},
    {"OSError", {"Exception", NULL}},
    {"BlockingIOError", {"OSError", NULL}},
    {"ChildProcessError", {"OSError", NULL}},
    {"ConnectionError", {"OSError", NULL}},
    {"BrokenPipeError", {"ConnectionError", NULL}},
    {"ConnectionAbortedError", {"ConnectionError", NULL}},
    {"ConnectionRefusedError", {"ConnectionError", NULL}},
    {"ConnectionResetError", {"ConnectionError", NULL}},
    {"FileExistsError", {"OSError", NULL}},
    {"FileNotFoundError", {"OSError", NULL}},
    {"InterruptedError", {"OSError", NULL}},
    {"IsADirectoryError", {"OSError", NULL}},
    {"NotADirectoryError", {"OSError", NULL}},
    {"PermissionError", {"OSError", NULL}},
    {"ProcessLookupError", {"OSError", NULL}},
    {"TimeoutError", {"OSError", NULL}},
    {"ReferenceError", {"Exception", NULL}},
    {"RuntimeError", {"Exception", NULL}},
    {"NotImplementedError", {"RuntimeError", NULL}},
    {"RecursionError", {"RuntimeError", NULL}},
    {"SyntaxError", {"Exception", NULL}},
    {"IndentationError", {"SyntaxError", NULL}},
    {"TabError", {"IndentationError", NULL}},
    {"SystemError", {"Exception", NULL}},
    {"TypeError", {"Exception", NULL}},
    {"ValueError", {"Exception", NULL}},
    {"UnicodeError", {"ValueError", NULL}},
    {"UnicodeDecodeError", {"UnicodeError", NULL}},
    {"UnicodeEncodeError", {"UnicodeError", NULL}},
    {"UnicodeTranslateError", {"UnicodeError", NULL}},
    {"JSONDecodeError", {"ValueError", NULL}},
    {"Warning", {"Exception", NULL}},
    {"DeprecationWarning", {"Warning", NULL}},
    {"PendingDeprecationWarning", {"Warning", NULL}},
    {"RuntimeWarning", {"Warning", NULL}},
    {"SyntaxWarning", {"Warning", NULL}},
    {"UserWarning", {"Warning", NULL}},
    {"FutureWarning", {"Warning", NULL}},
    {"ImportWarning", {"Warning", NULL}},
    {"UnicodeWarning", {"Warning", NULL}},
    {"BytesWarning", {"Warning", NULL}},
    {"ResourceWarning", {"Warning", NULL}},
    {"EncodingWarning", {"Warning", NULL}},
    /* quarry customs */
    {"queue.Empty", {"Exception", NULL}},
    {"CompressionLockHeld", {"Exception", NULL}},
    {"CompressionBusy", {"Exception", NULL}},
    {"SessionNotOwned", {"RuntimeError", NULL}},
    {"AuthError", {"RuntimeError", NULL}},
};
#define TMM_EXC_N (sizeof(TMM_EXC_PARENTS) / sizeof(TMM_EXC_PARENTS[0]))

static const tmm_exc_parent_t *tmm_exc_entry(const char *kind) {
    for (size_t i = 0; i < TMM_EXC_N; i++)
        if (!strcmp(TMM_EXC_PARENTS[i].kind, kind)) return &TMM_EXC_PARENTS[i];
    return NULL;
}

/* isinstance(kind, ancestor) over the parent tree; unknown kinds are
 * Exception subclasses (the make_exc fallback). */
static bool tmm_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const tmm_exc_parent_t *e = tmm_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (tmm_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmm_is_base_exc(const char *kind) {
    return kind && !tmm_exc_isa(kind, "Exception");
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmm_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = tmm_err_spec(type, msg);
    if (!strcmp(type, "CompressionLockHeld")) {
        /* live __init__(holder): str(exc) is derived from holder, and the
         * except site reads e.holder — carry it in the spec. */
        const json_t *holder = json_obj_get(spec, "holder");
        json_set(e, "holder", holder ? json_copy(holder) : json_null());
        char buf[512];
        const char *h = tmm_str(holder);
        snprintf(buf, sizeof buf, "Compression lock held: %s",
                 (h && h[0]) ? h : "unknown");
        json_set_hdr(e, "message", buf);
    }
    return e;
}

static json_t *tmm_call(tmm_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
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
        ret = tmm_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmm_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmm_make_exc(raise);
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

/* args builders (values borrowed; copied into the args object) */
static json_t *tmm_args0(void) { return json_object(); }
static void tmm_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmm_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmm_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmm_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── growable string buffer ── */
typedef struct tmm_sbuf { char *buf; size_t len, cap; } tmm_sbuf_t;

static void tmm_sb_init(tmm_sbuf_t *s) {
    s->cap = 128; s->len = 0;
    s->buf = malloc(s->cap);
    if (s->buf) s->buf[0] = '\0';
}
static void tmm_sb_grow(tmm_sbuf_t *s, size_t need) {
    if (!s->buf || s->len + need + 1 <= s->cap) return;
    while (s->cap < s->len + need + 1) s->cap *= 2;
    char *nb = realloc(s->buf, s->cap);
    if (nb) s->buf = nb;
}
static void tmm_sb_putc(tmm_sbuf_t *s, char c) {
    tmm_sb_grow(s, 1);
    if (!s->buf) return;
    s->buf[s->len++] = c;
    s->buf[s->len] = '\0';
}
static void tmm_sb_puts(tmm_sbuf_t *s, const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    tmm_sb_grow(s, n);
    if (!s->buf) return;
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = '\0';
}
static char *tmm_sb_take(tmm_sbuf_t *s) {
    char *r = s->buf ? s->buf : tmm_strdup("");
    s->buf = NULL; s->len = s->cap = 0;
    return r;
}

/* ── ASCII string helpers (fixture corpus is ASCII) ── */
static char *tmm_lower(const char *s) {
    char *o = tmm_strdup(s);
    for (char *p = o; p && *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static bool tmm_is_py_ws(char c) {
    /* str.strip() ASCII whitespace: space \t \n \r \v \f */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *tmm_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && tmm_is_py_ws(s[a])) a++;
    while (b > a && tmm_is_py_ws(s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static char *tmm_lstrip(const char *s, char ch) {
    if (!s) s = "";
    while (*s == ch) s++;
    return tmm_strdup(s);
}

static bool tmm_startswith(const char *s, const char *pfx) {
    if (!s || !pfx) return false;
    return strncmp(s, pfx, strlen(pfx)) == 0;
}

static bool tmm_endswith(const char *s, const char *sfx) {
    if (!s || !sfx) return false;
    size_t n = strlen(s), m = strlen(sfx);
    return n >= m && strcmp(s + n - m, sfx) == 0;
}

static bool tmm_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}

/* str.title() on ASCII: first alpha of each word upper, rest lower;
 * non-alpha breaks words (foo_bar -> Foo_Bar). */
static char *tmm_title(const char *s) {
    char *o = tmm_strdup(s);
    bool prev_alpha = false;
    for (char *p = o; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool alpha = isalpha(c) != 0;
        if (alpha) {
            *p = (char)(prev_alpha ? tolower(c) : toupper(c));
            prev_alpha = true;
        } else {
            prev_alpha = false;
        }
    }
    return o;
}

/* f"{n:,}" — thousands separators (ASCII digits). */
static char *tmm_fmt_comma(long long n) {
    char digits[64];
    snprintf(digits, sizeof digits, "%lld", n < 0 ? -n : n);
    size_t len = strlen(digits);
    char out[96];
    size_t o = 0;
    if (n < 0) out[o++] = '-';
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = digits[i];
    }
    out[o] = '\0';
    return tmm_strdup(out);
}

/* ── Python error builders (py3.14 messages, verified live) ── */
static json_t *tmm_key_error(const char *key) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s'", key ? key : "");
    return tmm_err_spec("KeyError", buf);
}

static json_t *tmm_attr_error(const char *type_name, const char *attr) {
    char buf[512];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'",
             type_name ? type_name : "object", attr ? attr : "");
    return tmm_err_spec("AttributeError", buf);
}

/* obj[key] on the dict/null fixture domain. Returns a BORROWED node or NULL
 * with *err set (KeyError / TypeError). */
static const json_t *tmm_sub(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (!obj || obj->type == JSON_NULL) {
        *err = tmm_err_spec("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        if (obj->type == JSON_ARRAY) {
            *err = tmm_err_spec("TypeError",
                "list indices must be integers or slices, not str");
        } else {
            char buf[256];
            snprintf(buf, sizeof buf, "'%s' object is not subscriptable",
                     tmm_type_name(obj));
            *err = tmm_err_spec("TypeError", buf);
        }
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) { *err = tmm_key_error(key); return NULL; }
    return v;
}

/* obj.get(key, default) — AttributeError on non-dict carriers. Returns a
 * BORROWED node (default when absent) or NULL with *err set. */
static const json_t *tmm_get(const json_t *obj, const char *key,
                             const json_t *def, json_t **err) {
    *err = NULL;
    if (!obj || obj->type != JSON_OBJECT) {
        *err = tmm_attr_error(tmm_type_name(obj), "get");
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    return v ? v : def;
}

/* getattr(obj, key, default) — ALWAYS safe with a default (None carries
 * every attribute read with a default). Borrowed node or the default. */
static const json_t *tmm_getattr_def(const json_t *obj, const char *key,
                                     const json_t *def) {
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
    }
    return def;
}

/* attribute read WITHOUT a default on a materialized double: a fixture dict
 * is a types.SimpleNamespace live; null/other carriers name their own type. */
static const json_t *tmm_attr_req(const json_t *obj, const char *key, json_t **err) {
    *err = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (v) return v;
        *err = tmm_attr_error("types.SimpleNamespace", key);
        return NULL;
    }
    *err = tmm_attr_error(tmm_type_name(obj), key);
    return NULL;
}

/* int(v) on the number/bool/null fixture domain (str parsing is outside the
 * domain). Returns the integer value; *err set on TypeError. */
static long long tmm_int(const json_t *v, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = tmm_err_spec("TypeError",
            "int() argument must be a string, a bytes-like object or a real "
            "number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    char buf[256];
    snprintf(buf, sizeof buf,
             "int() argument must be a string, a bytes-like object or a real "
             "number, not '%s'", tmm_type_name(v));
    *err = tmm_err_spec("TypeError", buf);
    return 0;
}

/* int(v or 0) */
static long long tmm_int_or0(const json_t *v, json_t **err) {
    if (!tmm_truthy(v)) { *err = NULL; return 0; }
    return tmm_int(v, err);
}

/* "\n".join(arr) with the live TypeError messages. arr may be an array, a
 * string (chars) or a dict (keys) — the iterable domain; scalars raise
 * "can only join an iterable". Items must be str. */
static char *tmm_join(const char *sep, const json_t *arr, json_t **err) {
    *err = NULL;
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    if (!arr || arr->type == JSON_NULL || arr->type == JSON_NUMBER ||
        arr->type == JSON_BOOL) {
        *err = tmm_err_spec("TypeError", "can only join an iterable");
        free(tmm_sb_take(&sb));
        return NULL;
    }
    size_t n = 0;
    json_t *const *items = NULL;
    char *chars = NULL;
    char **keys = NULL;
    if (arr->type == JSON_ARRAY) {
        n = arr->c.count; items = arr->c.items;
    } else if (arr->type == JSON_STRING) {
        chars = tmm_strdup(arr->str_val ? arr->str_val : "");
        n = strlen(chars);
    } else { /* dict: joins its keys */
        n = arr->c.count; keys = arr->c.keys;
    }
    for (size_t i = 0; i < n; i++) {
        const char *piece = NULL;
        char one[2] = {0, 0};
        if (items) {
            const json_t *it = items[i];
            if (!it || it->type != JSON_STRING) {
                char buf[256];
                snprintf(buf, sizeof buf,
                         "sequence item %zu: expected str instance, %s found",
                         i, tmm_type_name(it));
                *err = tmm_err_spec("TypeError", buf);
                break;
            }
            piece = it->str_val ? it->str_val : "";
        } else if (chars) {
            one[0] = chars[i]; piece = one;
        } else {
            piece = keys[i];
        }
        if (i > 0) tmm_sb_puts(&sb, sep);
        tmm_sb_puts(&sb, piece);
    }
    free(chars);
    if (*err) { free(tmm_sb_take(&sb)); return NULL; }
    return tmm_sb_take(&sb);
}

/* list += src (lines += account_limit_lines(...)): src must be an array in
 * the fixture domain; scalars raise "'T' object is not iterable". A string
 * would extend chars live — outside the domain. Appends COPIES. */
static void tmm_extend(json_t *dst, const json_t *src, json_t **err) {
    *err = NULL;
    if (!src || src->type != JSON_ARRAY) {
        char buf[256];
        snprintf(buf, sizeof buf, "'%s' object is not iterable",
                 tmm_type_name(src));
        *err = tmm_err_spec("TypeError", buf);
        return;
    }
    for (size_t i = 0; i < src->c.count; i++)
        json_append(dst, json_copy(src->c.items[i]));
}

/* a, b = value — the 2-unpack on the list domain. Borrowed out pointers. */
static void tmm_unpack2(const json_t *val, const json_t **a, const json_t **b,
                        json_t **err) {
    *err = NULL; *a = NULL; *b = NULL;
    if (!val || val->type != JSON_ARRAY) {
        char buf[256];
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object",
                 tmm_type_name(val));
        *err = tmm_err_spec("TypeError", buf);
        return;
    }
    if (val->c.count < 2) {
        char buf[128];
        snprintf(buf, sizeof buf,
                 "not enough values to unpack (expected 2, got %zu)", val->c.count);
        *err = tmm_err_spec("ValueError", buf);
        return;
    }
    if (val->c.count > 2) {
        char buf[128];
        snprintf(buf, sizeof buf,
                 "too many values to unpack (expected 2, got %zu)", val->c.count);
        *err = tmm_err_spec("ValueError", buf);
        return;
    }
    *a = val->c.items[0];
    *b = val->c.items[1];
}

/* value[1] — the route read. Borrowed node or *err. */
static const json_t *tmm_index1(const json_t *val, json_t **err) {
    *err = NULL;
    if (!val || val->type == JSON_NULL) {
        *err = tmm_err_spec("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (val->type == JSON_ARRAY) {
        if (val->c.count < 2) {
            *err = tmm_err_spec("IndexError", "list index out of range");
            return NULL;
        }
        return val->c.items[1];
    }
    if (val->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(val, "1");
        if (!v) { *err = tmm_err_spec("KeyError", "1"); return NULL; }
        return v;
    }
    if (val->type == JSON_STRING) {
        const char *s = val->str_val ? val->str_val : "";
        if (strlen(s) < 2) {
            *err = tmm_err_spec("IndexError", "string index out of range");
            return NULL;
        }
        /* a str[1] char is outside the fixture domain; model as error-free
         * single-char string would diverge on type — keep it unreachable */
        *err = tmm_err_spec("IndexError", "string index out of range");
        return NULL;
    }
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object is not subscriptable",
             tmm_type_name(val));
    *err = tmm_err_spec("TypeError", buf);
    return NULL;
}

/* {**fields, ...} — fields must be a mapping. */
static json_t *tmm_check_mapping(const json_t *fields) {
    if (fields && fields->type == JSON_OBJECT) return NULL;
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object is not a mapping",
             tmm_type_name(fields));
    return tmm_err_spec("TypeError", buf);
}

/* ── projections ── */
/* agent OBJECTS are always truthy live (SimpleNamespace); presence only. */
static bool tmm_agent_present(const json_t *agent) {
    return agent && agent->type != JSON_NULL;
}

static json_t *tmm_agent_marker(const json_t *agent) {
    return tmm_agent_present(agent) ? json_string("agent") : json_null();
}

/* proj_session: fixture session dict with agent -> "agent" marker. */
static json_t *tmm_proj_session(const json_t *session) {
    if (!session || session->type != JSON_OBJECT) return json_null();
    json_t *p = json_copy(session);
    const json_t *agent = json_obj_get(session, "agent");
    if (tmm_agent_present(agent)) json_set_hdr(p, "agent", "agent");
    return p;
}

/* str(x or "") — falsy collapses to the EMPTY STRING, not str(None). */
static char *tmm_str_or_empty(const json_t *v) {
    if (!tmm_truthy(v)) return tmm_strdup("");
    return tmm_pystr(v);
}

/* first truthy node of a NULL-terminated borrowed list, or NULL. */
static const json_t *tmm_first_truthy(const json_t *a, const json_t *b,
                                      const json_t *c, const json_t *d) {
    if (tmm_truthy(a)) return a;
    if (tmm_truthy(b)) return b;
    if (tmm_truthy(c)) return c;
    if (tmm_truthy(d)) return d;
    return NULL;
}

/* sorted({str(n) for ...}) — collect, sort, dedupe (ASCII strcmp order). */
static int tmm_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* history list copy: list(session.get("history", [])) — a non-list history
 * would raise live; the fixture domain is list/null/absent. */
static json_t *tmm_history_copy(const json_t *session) {
    const json_t *h = session ? json_obj_get(session, "history") : NULL;
    if (h && h->type == JSON_ARRAY) return json_copy(h);
    return json_array();
}

/* with session["history_lock"]: KeyError when the key is missing. The lock
 * itself is a silent CM (enter/exit fire no events). Returns NULL ok / err. */
static json_t *tmm_history_lock(const json_t *session) {
    json_t *err = NULL;
    tmm_sub(session, "history_lock", &err);
    return err;
}

/* ── live constants (verbatim from methods_slash.py / the manual module) ── */
#define TMM_NO_AGENT_USAGE "(._.) No active agent -- send a message first."
#define TMM_SEP40 \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" \
    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
#define TMM_AGGRESSIVE_UNSUPPORTED \
    "--aggressive is not supported; use '/compress here [N]' to keep only " \
    "recent exchanges, or /undo to drop turns."

static const char *TMM_ISOLATED_READ[] = {"context", "tools", "help", NULL};
static const char *TMM_CONTEXT_LOCAL[] = {"context", NULL};
static const char *TMM_MUTATES_RUNNING[] = {"model", "personality", "compress", NULL};

typedef struct tmm_fast_tier { const char *arg; const char *tier; } tmm_fast_tier_t;
static const tmm_fast_tier_t TMM_FAST_TIERS[] = {
    {"fast", "priority"}, {"on", "priority"}, {"normal", NULL}, {"off", NULL},
    {"auto", "auto"}, {"cold", "cold"}, {"ultrafast", "ultrafast"},
    {"ultra", "ultrafast"},
};

static bool tmm_in_list(const char *const *list, const char *s) {
    for (size_t i = 0; list[i]; i++)
        if (!strcmp(list[i], s)) return true;
    return false;
}

/* _FAST_TIERS lookup: returns the tier string, "" for a None value, NULL
 * when the arg is not a tier key. */
static const char *tmm_fast_tier(const char *arg) {
    for (size_t i = 0; i < sizeof(TMM_FAST_TIERS) / sizeof(TMM_FAST_TIERS[0]); i++)
        if (!strcmp(TMM_FAST_TIERS[i].arg, arg))
            return TMM_FAST_TIERS[i].tier ? TMM_FAST_TIERS[i].tier : "";
    return NULL;
}/* ── sibling-seam arg builders ── */
static json_t *tmm_sib_fmt3(tmm_ctx_t *ctx, const char *seam, const char *sid,
                            const json_t *session, const char *arg) {
    json_t *a = tmm_args0();
    tmm_asets(a, "sid", sid);
    tmm_aset(a, "session", tmm_proj_session(session));
    tmm_asets(a, "arg", arg);
    return tmm_call(ctx, seam, a);
}

static json_t *tmm_sib_session(tmm_ctx_t *ctx, const char *seam,
                               const json_t *session) {
    json_t *a = tmm_args0();
    tmm_aset(a, "session", tmm_proj_session(session));
    return tmm_call(ctx, seam, a);
}

static json_t *tmm_sib_mirror4(tmm_ctx_t *ctx, const char *seam, const char *sid,
                               const json_t *session, const json_t *agent,
                               const char *arg) {
    json_t *a = tmm_args0();
    tmm_asets(a, "sid", sid);
    tmm_aset(a, "session", tmm_proj_session(session));
    tmm_aset(a, "agent", tmm_agent_marker(agent));
    tmm_asets(a, "arg", arg);
    return tmm_call(ctx, seam, a);
}

/* ══ _usage_route ══ */
/* PoP: tmm_usage_route @ tui_gateway/methods_slash.py:_usage_route */
static json_t *tmm_usage_route(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    if (tmm_agent_present(agent)) {
        json_t *ret = json_object();
        json_set(ret, "provider", json_copy(tmm_getattr_def(agent, "provider", NULL)));
        json_set(ret, "base_url", json_copy(tmm_getattr_def(agent, "base_url", NULL)));
        json_set(ret, "api_key", json_copy(tmm_getattr_def(agent, "api_key", NULL)));
        return ret;
    }
    json_t *build = tmm_sib_session(ctx, "deferred_build_agent_kwargs", session);
    /* the live call is _deferred_build_agent_kwargs(session, None) — an
     * external server global, not a ported sibling */
    if (tmm_is_raise(build)) return build;
    const json_t *mo = tmm_get(build, "model_override", NULL, &err);
    if (err) { json_free(build); return err; }
    const json_t *po = tmm_get(build, "provider_override", NULL, &err);
    if (err) { json_free(build); return err; }
    json_t *a = tmm_args0();
    tmm_aset(a, "model_override", mo);
    tmm_aset(a, "provider_override", po);
    json_t *r = tmm_call(ctx, "resolve_agent_model_runtime", a);
    json_free(build);
    if (tmm_is_raise(r)) {
        const char *k = tmm_raise_kind(r);
        if (tmm_exc_isa(k, "RuntimeError") || tmm_exc_isa(k, "ValueError")) {
            json_free(r);
            return json_object();
        }
        return r;
    }
    const json_t *route = tmm_index1(r, &err);
    if (err) { json_free(r); return err; }
    json_t *ret = json_copy(route);
    json_free(r);
    return ret;
}

/* ══ _usage_on_nous_route ══ */
/* PoP: tmm_usage_on_nous_route @ tui_gateway/methods_slash.py:_usage_on_nous_route */
static json_t *tmm_usage_on_nous_route(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *route = json_obj_get(args, "route");
    json_t *err = NULL;
    const json_t *provider = tmm_get(route, "provider", NULL, &err);
    if (err) return err;
    const json_t *base_url = tmm_get(route, "base_url", NULL, &err);
    if (err) return err;
    char *p = tmm_str_or_empty(provider);
    char *b = tmm_str_or_empty(base_url);
    json_t *a = tmm_args0();
    tmm_asets(a, "provider", p);
    tmm_asets(a, "base_url", b);
    free(p); free(b);
    return tmm_call(ctx, "is_nous_inference_route", a);
}

/* ══ _usage_nous_fields ══ */
/* PoP: tmm_usage_nous_fields @ tui_gateway/methods_slash.py:_usage_nous_fields */
static json_t *tmm_usage_nous_fields(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
    if (tmm_is_raise(scope)) return scope;
    json_free(scope);
    json_t *route = tmm_sib_session(ctx, "usage_route", session);
    if (tmm_is_raise(route)) return route;
    json_t *a = tmm_args0();
    tmm_aset(a, "route", route);
    json_t *on_nous = tmm_call(ctx, "usage_on_nous_route", a);
    json_free(route);
    if (tmm_is_raise(on_nous)) return on_nous;
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) { json_free(on_nous); return err; }
    json_t *a2 = tmm_args0();
    tmm_aset(a2, "agent", tmm_agent_marker(agent));
    tmm_aset(a2, "nous_route", on_nous);
    json_t *fields = tmm_call(ctx, "nous_credits_usage_fields", a2);
    if (tmm_is_raise(fields)) { json_free(on_nous); return fields; }
    json_t *map_err = tmm_check_mapping(fields);
    if (map_err) { json_free(fields); json_free(on_nous); return map_err; }
    json_set(fields, "nous_route", on_nous);   /* adopts on_nous */
    return fields;
}

/* ══ _usage_account_blocks ══ */
/* PoP: tmm_usage_account_blocks @ tui_gateway/methods_slash.py:_usage_account_blocks */
static json_t *tmm_usage_account_blocks(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    json_t *lines = json_array();
    json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
    if (tmm_is_raise(scope)) { json_free(lines); return scope; }
    json_free(scope);
    json_t *route = tmm_sib_session(ctx, "usage_route", session);
    if (tmm_is_raise(route)) { json_free(lines); return route; }

    /* with contextlib.suppress(Exception): lines += account_limit_lines(...) */
    const json_t *provider = tmm_get(route, "provider", NULL, &err);
    if (!err) {
        const json_t *base_url = tmm_get(route, "base_url", NULL, &err);
        if (!err) {
            const json_t *api_key = tmm_get(route, "api_key", NULL, &err);
            if (!err) {
                json_t *a = tmm_args0();
                tmm_aset(a, "provider", provider);
                tmm_aset(a, "base_url", base_url);
                tmm_aset(a, "api_key", api_key);
                json_t *r = tmm_call(ctx, "account_limit_lines", a);
                if (tmm_is_raise(r)) {
                    if (tmm_is_base_exc(tmm_raise_kind(r))) {
                        json_free(lines); json_free(route); return r;
                    }
                    json_free(r);
                } else {
                    tmm_extend(lines, r, &err);
                    json_free(r);
                    if (err) {
                        if (tmm_is_base_exc(tmm_raise_kind(err))) {
                            json_free(lines); json_free(route); return err;
                        }
                        json_free(err); err = NULL;
                    }
                }
            }
        }
    }
    if (err) {
        if (tmm_is_base_exc(tmm_raise_kind(err))) {
            json_free(lines); json_free(route); return err;
        }
        json_free(err); err = NULL;
    }

    /* second suppress: state/window, then nous_usage_lines(nous_route=...) */
    const json_t *state = tmm_getattr_def(agent, "_credits_state", NULL);
    bool window = false;
    if (state && state->type == JSON_OBJECT) {
        const json_t *iw = json_obj_get(state, "is_usage_window");
        window = iw && iw->type == JSON_BOOL && iw->bool_val;   /* `is True` */
    }
    json_t *a2 = tmm_args0();
    tmm_aset(a2, "route", route);
    json_t *on_nous = tmm_call(ctx, "usage_on_nous_route", a2);
    if (tmm_is_raise(on_nous)) {
        if (tmm_is_base_exc(tmm_raise_kind(on_nous))) {
            json_free(lines); json_free(route); return on_nous;
        }
        json_free(on_nous);
    } else {
        json_t *a3 = tmm_args0();
        tmm_aset(a3, "nous_route", on_nous);
        tmm_aset(a3, "credits_state", window ? state : NULL);
        json_t *r2 = tmm_call(ctx, "nous_usage_lines", a3);
        json_free(on_nous);
        if (tmm_is_raise(r2)) {
            if (tmm_is_base_exc(tmm_raise_kind(r2))) {
                json_free(lines); json_free(route); return r2;
            }
            json_free(r2);
        } else {
            tmm_extend(lines, r2, &err);
            json_free(r2);
            if (err) {
                if (tmm_is_base_exc(tmm_raise_kind(err))) {
                    json_free(lines); json_free(route); return err;
                }
                json_free(err); err = NULL;
            }
        }
    }
    json_free(route);
    return lines;
}

/* ══ _format_live_usage_tokens ══ */
/* PoP: tmm_format_live_usage_tokens @ tui_gateway/methods_slash.py:_format_live_usage_tokens */
static json_t *tmm_format_live_usage_tokens(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    json_t *usage = tmm_sib_session(ctx, "session_usage_snapshot", session);
    if (tmm_is_raise(usage)) return usage;
    if (!tmm_agent_present(agent) && !tmm_truthy(usage)) {
        json_free(usage);
        return json_string("");
    }
    long long message_count = 0;
    const json_t *mmc = tmm_get(session, "_metadata_message_count", NULL, &err);
    if (err) { json_free(usage); return err; }
    if (mmc && mmc->type != JSON_NULL) {
        message_count = tmm_int_or0(mmc, &err);
        if (err) { json_free(usage); return err; }
    } else {
        json_t *lock_err = tmm_history_lock(session);
        if (lock_err) { json_free(usage); return lock_err; }
        json_t *h = tmm_history_copy(session);
        message_count = (long long)json_len(h);
        json_free(h);
    }
    /* n(key) = f"{int(usage.get(key) or 0):,}" */
    #define TMM_N(key, out) do { \
        const json_t *nv = tmm_get(usage, key, NULL, &err); \
        if (err) goto done; \
        long long nn = tmm_int_or0(nv, &err); \
        if (err) goto done; \
        out = tmm_fmt_comma(nn); \
    } while (0)
    char *n_input = NULL, *n_output = NULL, *n_reasoning = NULL,
         *n_prompt = NULL, *n_completion = NULL, *n_total = NULL,
         *n_calls = NULL, *n_ctx_used = NULL, *n_ctx_max = NULL,
         *n_compressions = NULL;
    bool have_reasoning = false, have_context = false;
    long long pct = 0;
    const char *mark = "";
    TMM_N("input", n_input);
    TMM_N("output", n_output);
    {
        const json_t *rv = tmm_get(usage, "reasoning", NULL, &err);
        if (err) goto done;
        long long rn = tmm_int_or0(rv, &err);
        if (err) goto done;
        if (rn) { have_reasoning = true; TMM_N("reasoning", n_reasoning); }
    }
    TMM_N("prompt", n_prompt);
    TMM_N("completion", n_completion);
    TMM_N("total", n_total);
    TMM_N("calls", n_calls);
    {
        const json_t *cm = tmm_get(usage, "context_max", NULL, &err);
        if (err) goto done;
        if (tmm_truthy(cm)) {
            have_context = true;
            const json_t *pv = tmm_get(usage, "context_percent", NULL, &err);
            if (err) goto done;
            pct = tmm_int_or0(pv, &err);
            if (err) goto done;
            const json_t *est = tmm_get(usage, "context_estimated", NULL, &err);
            if (err) goto done;
            mark = tmm_truthy(est) ? "~" : "";
            TMM_N("context_used", n_ctx_used);
            TMM_N("context_max", n_ctx_max);
        }
    }
    TMM_N("compressions", n_compressions);
    {
        /* model = usage.model or _metadata_mirror(session).model or
         *         getattr(agent, "model", "") or "(unknown)" */
        const json_t *um = tmm_get(usage, "model", NULL, &err);
        if (err) goto done;
        const json_t *model_node = NULL;
        char *model_str = NULL;
        if (tmm_truthy(um)) {
            model_str = tmm_pystr(um);
        } else {
            json_t *mirror = tmm_sib_session(ctx, "metadata_mirror", session);
            if (tmm_is_raise(mirror)) { err = mirror; goto done; }
            const json_t *mm = tmm_get(mirror, "model", NULL, &err);
            if (err) { json_free(mirror); goto done; }
            if (tmm_truthy(mm)) {
                model_str = tmm_pystr(mm);
            } else {
                const json_t *am = tmm_getattr_def(agent, "model", NULL);
                model_node = am;
                if (tmm_truthy(am)) model_str = tmm_pystr(am);
                else model_str = tmm_strdup("(unknown)");
            }
            json_free(mirror);
        }
        (void)model_node;
        char *mc = tmm_fmt_comma(message_count);
        tmm_sbuf_t sb; tmm_sb_init(&sb);
        tmm_sb_puts(&sb, "Session Token Usage\n");
        tmm_sb_puts(&sb, TMM_SEP40 "\n");
        tmm_sb_puts(&sb, "Model: "); tmm_sb_puts(&sb, model_str); tmm_sb_putc(&sb, '\n');
        char row[512];
        snprintf(row, sizeof row, "%-30s%s\n", "Input tokens:", n_input);
        tmm_sb_puts(&sb, row);
        snprintf(row, sizeof row, "%-30s%s\n", "Output tokens:", n_output);
        tmm_sb_puts(&sb, row);
        if (have_reasoning) {
            snprintf(row, sizeof row, "%-30s%s\n", "Reasoning tokens:", n_reasoning);
            tmm_sb_puts(&sb, row);
        }
        snprintf(row, sizeof row, "%-30s%s\n", "Prompt tokens:", n_prompt);
        tmm_sb_puts(&sb, row);
        snprintf(row, sizeof row, "%-30s%s\n", "Completion tokens:", n_completion);
        tmm_sb_puts(&sb, row);
        snprintf(row, sizeof row, "%-30s%s\n", "Total tokens:", n_total);
        tmm_sb_puts(&sb, row);
        snprintf(row, sizeof row, "%-30s%s\n", "API calls:", n_calls);
        tmm_sb_puts(&sb, row);
        if (have_context) {
            char val[256];
            snprintf(val, sizeof val, "%s%s / %s (%s%lld%%)",
                     mark, n_ctx_used, n_ctx_max, mark, pct);
            snprintf(row, sizeof row, "%-30s%s\n", "Current context:", val);
            tmm_sb_puts(&sb, row);
        }
        snprintf(row, sizeof row, "%-30s%s\n", "Messages:", mc);
        tmm_sb_puts(&sb, row);
        snprintf(row, sizeof row, "%-30s%s", "Compressions:", n_compressions);
        tmm_sb_puts(&sb, row);
        free(mc); free(model_str);
        char *joined = tmm_sb_take(&sb);
        err = NULL;
        json_t *ret = json_string(joined);
        free(joined);
        goto done_free;
    }
done:
    /* err set (or a seam raise parked in err) */
    ;
    json_t *ret = err ? err : json_string("");
done_free:
    free(n_input); free(n_output); free(n_reasoning); free(n_prompt);
    free(n_completion); free(n_total); free(n_calls); free(n_ctx_used);
    free(n_ctx_max); free(n_compressions);
    json_free(usage);
    return ret;
    #undef TMM_N
}

/* ══ _format_live_usage_output ══ */
/* PoP: tmm_format_live_usage_output @ tui_gateway/methods_slash.py:_format_live_usage_output */
static json_t *tmm_format_live_usage_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *sid = json_get_str(args, "sid", "");
    const char *arg = json_get_str(args, "arg", "");
    (void)sid; (void)arg;
    json_t *extra = tmm_sib_session(ctx, "usage_account_blocks", session);
    if (tmm_is_raise(extra)) return extra;
    json_t *tokens = tmm_sib_session(ctx, "format_live_usage_tokens", session);
    if (tmm_is_raise(tokens)) { json_free(extra); return tokens; }
    json_t *err = NULL;
    char *joined_extra = tmm_join("\n", extra, &err);
    json_free(extra);
    if (err) { json_free(tokens); return err; }
    /* "\n\n".join(filter(None, [tokens, joined_extra])) or _NO_AGENT_USAGE */
    json_t *parts = json_array();
    if (tmm_truthy(tokens)) json_append(parts, json_copy(tokens));
    if (joined_extra[0]) json_append(parts, json_string(joined_extra));
    free(joined_extra);
    char *joined = tmm_join("\n\n", parts, &err);
    json_free(parts);
    json_free(tokens);
    if (err) return err;
    json_t *ret = json_string(joined[0] ? joined : TMM_NO_AGENT_USAGE);
    free(joined);
    return ret;
}

/* ══ _format_live_model_output ══ */
/* PoP: tmm_format_live_model_output @ tui_gateway/methods_slash.py:_format_live_model_output */
static json_t *tmm_format_live_model_output(tmm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *session = json_obj_get(args, "session");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    const json_t *model = tmm_agent_present(agent)
        ? tmm_getattr_def(agent, "model", NULL) : NULL;
    const json_t *provider = tmm_agent_present(agent)
        ? tmm_getattr_def(agent, "provider", NULL) : NULL;
    if (!tmm_truthy(model)) return json_string("Current model: (unknown)");
    char *m = tmm_pystr(model);
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    tmm_sb_puts(&sb, "Current model: ");
    tmm_sb_puts(&sb, m);
    free(m);
    if (tmm_truthy(provider)) {
        char *p = tmm_pystr(provider);
        tmm_sb_puts(&sb, " (");
        tmm_sb_puts(&sb, p);
        tmm_sb_puts(&sb, ")");
        free(p);
    }
    char *s = tmm_sb_take(&sb);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* ══ _format_live_status_output ══ */
/* PoP: tmm_format_live_status_output @ tui_gateway/methods_slash.py:_format_live_status_output */
static json_t *tmm_format_live_status_output(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    json_t *params = json_object();
    json_set_hdr(params, "session_id", sid);
    json_t *a = tmm_args0();
    tmm_asets(a, "rid", "status");
    tmm_aset(a, "params", params);
    json_free(params);
    json_t *response = tmm_call(ctx, "methods.session_status", a);
    if (tmm_is_raise(response)) return response;
    json_t *err = NULL;
    const json_t *error = tmm_get(response, "error", NULL, &err);
    if (err) { json_free(response); return err; }
    json_t *ret = NULL;
    if (tmm_truthy(error)) {
        const json_t *msg = tmm_get(error, "message", NULL, &err);
        if (err) { json_free(response); return err; }
        char *s = tmm_truthy(msg) ? tmm_pystr(msg) : tmm_strdup("status unavailable");
        ret = json_string(s);
        free(s);
    } else {
        json_t *empty = json_object();
        const json_t *result = tmm_get(response, "result", empty, &err);
        if (err) { json_free(empty); json_free(response); return err; }
        if (result != empty) json_free(empty);
        const json_t *output = tmm_get(result, "output", NULL, &err);
        if (err) {
            if (result == empty) json_free(empty);
            json_free(response);
            return err;
        }
        char *s = tmm_str_or_empty(output);
        if (result == empty) json_free(empty);
        ret = json_string(s);
        free(s);
    }
    json_free(response);
    return ret;
}

/* ══ _format_live_tools_output ══ */
/* PoP: tmm_format_live_tools_output @ tui_gateway/methods_slash.py:_format_live_tools_output */
static json_t *tmm_format_live_tools_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    json_t *a = tmm_args0();
    tmm_aset(a, "agent", tmm_agent_marker(agent));
    tmm_aset(a, "session", tmm_proj_session(session));
    json_t *info = tmm_call(ctx, "session_info", a);
    if (tmm_is_raise(info)) return info;
    const json_t *groups = NULL;
    if (info && info->type == JSON_OBJECT) groups = json_obj_get(info, "tools");
    if (!groups || groups->type != JSON_OBJECT || groups->c.count == 0) {
        json_free(info);
        return json_string("No tools available.");
    }
    /* names = sorted({str(n) for g in groups.values() if isinstance(g, list)
     *                 for n in g}) */
    char **names = NULL;
    size_t n_names = 0, cap = 0;
    for (size_t i = 0; i < groups->c.count; i++) {
        const json_t *g = groups->c.items[i];
        if (!g || g->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < g->c.count; j++) {
            char *s = tmm_pystr(g->c.items[j]);
            if (n_names == cap) {
                cap = cap ? cap * 2 : 16;
                names = realloc(names, cap * sizeof(char *));
            }
            names[n_names++] = s;
        }
    }
    json_free(info);
    if (n_names == 0) {
        free(names);
        return json_string("No tools available.");
    }
    qsort(names, n_names, sizeof(char *), tmm_str_cmp);
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    char hdr[64];
    /* dedupe while counting */
    size_t uniq = 0;
    for (size_t i = 0; i < n_names; i++) {
        if (i > 0 && !strcmp(names[i], names[i - 1])) continue;
        uniq++;
    }
    snprintf(hdr, sizeof hdr, "Available tools (%zu):\n", uniq);
    tmm_sb_puts(&sb, hdr);
    bool first = true;
    for (size_t i = 0; i < n_names; i++) {
        if (i > 0 && !strcmp(names[i], names[i - 1])) continue;
        if (!first) tmm_sb_putc(&sb, '\n');
        first = false;
        tmm_sb_puts(&sb, "  ");
        tmm_sb_puts(&sb, names[i]);
    }
    for (size_t i = 0; i < n_names; i++) free(names[i]);
    free(names);
    char *s = tmm_sb_take(&sb);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* ══ _format_live_help_output ══ */
/* PoP: tmm_format_live_help_output @ tui_gateway/methods_slash.py:_format_live_help_output */
static json_t *tmm_format_live_help_output(tmm_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cat = tmm_call(ctx, "commands_by_category", tmm_args0());
    if (tmm_is_raise(cat)) {
        if (tmm_is_base_exc(tmm_raise_kind(cat))) return cat;
        char *s = tmm_strdup(json_get_str(cat, "message", ""));
        json_free(cat);
        tmm_sbuf_t sb; tmm_sb_init(&sb);
        tmm_sb_puts(&sb, "help unavailable: ");
        tmm_sb_puts(&sb, s);
        free(s);
        char *out = tmm_sb_take(&sb);
        json_t *ret = json_string(out);
        free(out);
        return ret;
    }
    /* COMMANDS_BY_CATEGORY.items() — a non-dict carrier raises AttributeError
     * caught by the same except */
    json_t *err = NULL;
    if (!cat || cat->type != JSON_OBJECT) {
        err = tmm_attr_error(tmm_type_name(cat), "items");
    }
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    if (!err) {
        tmm_sb_puts(&sb, "Available commands:\n\n");
        for (size_t i = 0; i < cat->c.count && !err; i++) {
            const char *category = cat->c.keys[i];
            const json_t *commands = cat->c.items[i];
            tmm_sb_puts(&sb, category);
            tmm_sb_puts(&sb, ":\n");
            if (!commands || commands->type != JSON_OBJECT) {
                err = tmm_attr_error(tmm_type_name(commands), "items");
                break;
            }
            for (size_t j = 0; j < commands->c.count; j++) {
                char line[1024];
                char *desc = tmm_pystr(commands->c.items[j]);
                snprintf(line, sizeof line, "  %-15s %s\n", commands->c.keys[j], desc);
                free(desc);
                tmm_sb_puts(&sb, line);
            }
        }
    }
    json_free(cat);
    if (err) {
        if (tmm_is_base_exc(tmm_raise_kind(err))) {
            free(tmm_sb_take(&sb));
            return err;
        }
        tmm_sbuf_t eb; tmm_sb_init(&eb);
        tmm_sb_puts(&eb, "help unavailable: ");
        tmm_sb_puts(&eb, json_get_str(err, "message", ""));
        json_free(err);
        free(tmm_sb_take(&sb));
        char *out = tmm_sb_take(&eb);
        json_t *ret = json_string(out);
        free(out);
        return ret;
    }
    /* strip the trailing newline of the last appended line: live joins a
     * list of lines with "\n" (no trailing separator) */
    char *s = tmm_sb_take(&sb);
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[n - 1] = '\0';
    json_t *ret = json_string(s);
    free(s);
    return ret;
}/* ══ _live_session_messages ══ */
/* PoP: tmm_live_session_messages @ tui_gateway/methods_slash.py:_live_session_messages */
static json_t *tmm_live_session_messages(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *db = tmm_sib_session(ctx, "session_db", session);
    if (tmm_is_raise(db)) return db;
    json_t *a = tmm_args0();
    tmm_aset(a, "db", db);
    json_t *p = tmm_call(ctx, "persistence_for", a);
    json_free(db);
    if (tmm_is_raise(p)) return p;
    json_t *ret = json_null();
    json_t *err = NULL;
    const json_t *key = tmm_get(session, "session_key", NULL, &err);
    if (err) { json_free(p); return err; }
    if (tmm_agent_present(p) && tmm_truthy(key)) {
        /* with contextlib.suppress(Exception): return [m.as_openai_message()
         *     for m in persistence.messages.read_conversation(
         *         session["session_key"], ConversationRequest(...))] */
        const json_t *ref = tmm_sub(session, "session_key", &err);
        if (err) {
            if (tmm_is_base_exc(tmm_raise_kind(err))) { json_free(p); return err; }
            json_free(err);
            goto out;
        }
        json_t *a2 = tmm_args0();
        tmm_asets(a2, "transcript_ref", tmm_str(ref));
        tmm_asetb(a2, "include_lineage", true);
        tmm_asetb(a2, "include_row_ids", true);
        json_t *msgs = tmm_call(ctx, "persistence.read_conversation", a2);
        if (tmm_is_raise(msgs)) {
            if (tmm_is_base_exc(tmm_raise_kind(msgs))) { json_free(p); return msgs; }
            json_free(msgs);
            goto out;
        }
        if (!msgs || msgs->type != JSON_ARRAY) {
            /* for m in msgs — TypeError suppressed */
            json_free(msgs);
            goto out;
        }
        json_t *out_arr = json_array();
        for (size_t i = 0; i < msgs->c.count; i++) {
            json_t *m = tmm_call(ctx, "message.as_openai_message", tmm_args0());
            if (tmm_is_raise(m)) {
                if (tmm_is_base_exc(tmm_raise_kind(m))) {
                    json_free(out_arr); json_free(msgs); json_free(p);
                    return m;
                }
                json_free(m);
                json_free(out_arr);
                json_free(msgs);
                goto out;
            }
            json_append(out_arr, m);
        }
        json_free(msgs);
        json_free(ret);
        ret = out_arr;
    }
out:
    json_free(p);
    return ret;
}

/* ══ _format_live_history_output ══ */
/* PoP: tmm_format_live_history_output @ tui_gateway/methods_slash.py:_format_live_history_output */
static json_t *tmm_format_live_history_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    json_t *lock_err = tmm_history_lock(session);
    if (lock_err) return lock_err;
    json_t *history = tmm_history_copy(session);
    json_t *db_history = tmm_sib_session(ctx, "live_session_messages", session);
    if (tmm_is_raise(db_history)) { json_free(history); return db_history; }
    /* history if db_history is None else db_history */
    const json_t *selected =
        (!db_history || db_history->type == JSON_NULL) ? history : db_history;
    json_t *err = NULL;
    const json_t *ph = tmm_get(session, "profile_home", NULL, &err);
    if (err) { json_free(history); json_free(db_history); return err; }
    json_t *a = tmm_args0();
    tmm_aset(a, "history", selected);
    tmm_aset(a, "profile_home", ph);
    json_t *messages = tmm_call(ctx, "history_to_messages", a);
    json_free(history);
    json_free(db_history);
    if (tmm_is_raise(messages)) return messages;
    if (!tmm_truthy(messages)) {
        json_free(messages);
        return json_string("No conversation history yet.");
    }
    if (messages->type != JSON_ARRAY) {
        char buf[256];
        snprintf(buf, sizeof buf, "'%s' object is not iterable",
                 tmm_type_name(messages));
        json_free(messages);
        return tmm_err_spec("TypeError", buf);
    }
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    tmm_sb_puts(&sb, "Conversation History\n");
    tmm_sb_puts(&sb, TMM_SEP40);
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *msg = messages->c.items[i];
        const json_t *role_node = tmm_get(msg, "role", NULL, &err);
        if (err) break;
        char *role = tmm_truthy(role_node) ? tmm_pystr(role_node) : tmm_strdup("unknown");
        char *label = NULL;
        if (!strcmp(role, "user")) label = tmm_strdup("You");
        else if (!strcmp(role, "assistant")) label = tmm_strdup("Hermes");
        else label = tmm_title(role);
        free(role);
        const json_t *text_node = tmm_get(msg, "text", NULL, &err);
        if (err) { free(label); break; }
        if (!tmm_truthy(text_node)) {
            text_node = tmm_get(msg, "context", NULL, &err);
            if (err) { free(label); break; }
        }
        char *raw = tmm_str_or_empty(text_node);
        char *text = tmm_strip(raw);
        free(raw);
        char *shown = NULL;
        if (strlen(text) > 400) {
            shown = malloc(404);
            memcpy(shown, text, 400);
            memcpy(shown + 400, "...", 4);
        } else {
            shown = tmm_strdup(text);
        }
        free(text);
        tmm_sb_putc(&sb, '\n');
        tmm_sb_putc(&sb, '[');
        tmm_sb_puts(&sb, label);
        free(label);
        char idxbuf[32];
        snprintf(idxbuf, sizeof idxbuf, " #%zu] ", i + 1);
        tmm_sb_puts(&sb, idxbuf);
        tmm_sb_puts(&sb, shown[0] ? shown : "(no text)");
        free(shown);
    }
    json_free(messages);
    if (err) {
        free(tmm_sb_take(&sb));
        return err;
    }
    char *s = tmm_sb_take(&sb);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* role Counter over the messages list: counts for the four reported roles. */
typedef struct tmm_roles { long long user, assistant, tool, system; } tmm_roles_t;

static json_t *tmm_count_roles(const json_t *messages, tmm_roles_t *roles,
                               json_t **err) {
    *err = NULL;
    roles->user = roles->assistant = roles->tool = roles->system = 0;
    if (!messages || messages->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *msg = messages->c.items[i];
        const json_t *role_node = tmm_get(msg, "role", NULL, err);
        if (*err) return NULL;
        char *role = tmm_truthy(role_node) ? tmm_pystr(role_node) : tmm_strdup("unknown");
        if (!strcmp(role, "user")) roles->user++;
        else if (!strcmp(role, "assistant")) roles->assistant++;
        else if (!strcmp(role, "tool")) roles->tool++;
        else if (!strcmp(role, "system")) roles->system++;
        free(role);
    }
    return NULL;
}

/* ══ _format_live_context_output ══ */
/* PoP: tmm_format_live_context_output @ tui_gateway/methods_slash.py:_format_live_context_output */
static json_t *tmm_format_live_context_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *arg = json_get_str(args, "arg", "");
    json_t *err = NULL;
    json_t *messages = NULL;
    json_t *breakdown_lines = json_array();
    json_t *lines = json_array();
    json_t *ret = NULL;

    /* try: messages = _history_to_messages(_live_session_messages(session)
     *      or [], profile_home=...) except Exception: messages = [] */
    {
        json_t *db = tmm_sib_session(ctx, "live_session_messages", session);
        if (tmm_is_raise(db)) {
            if (tmm_is_base_exc(tmm_raise_kind(db))) { ret = db; goto cleanup; }
            json_free(db);
            messages = json_array();
        } else {
            json_t *sel = (!db || db->type == JSON_NULL) ? json_array() : db;
            const json_t *ph = tmm_get(session, "profile_home", NULL, &err);
            if (err) {
                if (sel != db) json_free(sel);
                json_free(db);
                if (tmm_is_base_exc(tmm_raise_kind(err))) { ret = err; goto cleanup; }
                json_free(err); err = NULL;
                messages = json_array();
            } else {
                json_t *a = tmm_args0();
                tmm_aset(a, "history", sel);
                tmm_aset(a, "profile_home", ph);
                json_t *m = tmm_call(ctx, "history_to_messages", a);
                if (sel != db) json_free(sel);
                json_free(db);
                if (tmm_is_raise(m)) {
                    if (tmm_is_base_exc(tmm_raise_kind(m))) { ret = m; goto cleanup; }
                    json_free(m);
                    messages = json_array();
                } else {
                    messages = m;
                }
            }
        }
    }
    if (!tmm_truthy(messages)) {
        json_t *lock_err = tmm_history_lock(session);
        if (lock_err) { ret = lock_err; goto cleanup; }
        json_t *h = tmm_history_copy(session);
        const json_t *ph = tmm_get(session, "profile_home", NULL, &err);
        if (err) { json_free(h); ret = err; goto cleanup; }
        json_t *a = tmm_args0();
        tmm_aset(a, "history", h);
        tmm_aset(a, "profile_home", ph);
        json_free(h);
        json_t *m = tmm_call(ctx, "history_to_messages", a);
        if (tmm_is_raise(m)) { ret = m; goto cleanup; }
        json_free(messages);
        messages = m;
    }

    /* live-agent breakdown block */
    {
        const json_t *agent = tmm_get(session, "agent", NULL, &err);
        if (err) { ret = err; goto cleanup; }
        if (tmm_agent_present(agent)) {
            json_t *lock_err = tmm_history_lock(session);
            if (lock_err) { ret = lock_err; goto cleanup; }
            json_t *live_messages = tmm_history_copy(session);
            /* outer try: tokens = _set_session_context(session["session_key"],
             *              cwd=_session_cwd(session)) */
            json_t *block_err = NULL;
            const json_t *skey = tmm_sub(session, "session_key", &block_err);
            json_t *tokens = NULL;
            if (!block_err) {
                json_t *cwd = tmm_sib_session(ctx, "session_cwd", session);
                if (tmm_is_raise(cwd)) {
                    block_err = cwd;
                } else {
                    json_t *a = tmm_args0();
                    tmm_asets(a, "session_key", tmm_str(skey));
                    tmm_aset(a, "cwd", cwd);
                    json_free(cwd);
                    tokens = tmm_call(ctx, "set_session_context", a);
                    if (tmm_is_raise(tokens)) { block_err = tokens; tokens = NULL; }
                }
            }
            if (!block_err) {
                /* inner try / finally: clear_session_context(tokens) */
                json_t *inner_err = NULL;
                json_t *a = tmm_args0();
                tmm_aset(a, "agent", tmm_agent_marker(agent));
                tmm_aset(a, "live_messages", live_messages);
                json_t *payload = tmm_call(ctx, "compute_session_context_breakdown", a);
                json_t *details = json_null();
                if (tmm_is_raise(payload)) {
                    inner_err = payload;
                } else {
                    char *al = tmm_strip(arg);
                    char *all = tmm_lower(al);
                    free(al);
                    bool want_details = !strcmp(all, "all") || !strcmp(all, "full")
                                        || !strcmp(all, "details");
                    free(all);
                    if (want_details) {
                        json_t *a2 = tmm_args0();
                        tmm_aset(a2, "agent", tmm_agent_marker(agent));
                        json_t *d = tmm_call(ctx, "compute_context_details", a2);
                        if (tmm_is_raise(d)) {
                            if (tmm_is_base_exc(tmm_raise_kind(d))) {
                                inner_err = d;
                            } else {
                                json_free(d);   /* contextlib.suppress */
                            }
                        } else {
                            json_free(details);
                            details = d;
                        }
                    }
                    if (!inner_err) {
                        json_t *a3 = tmm_args0();
                        tmm_aset(a3, "payload", payload);
                        tmm_aset(a3, "details", details);
                        tmm_asetb(a3, "grid", false);
                        json_t *bl = tmm_call(ctx, "render_context_breakdown_lines", a3);
                        if (tmm_is_raise(bl)) {
                            inner_err = bl;
                        } else {
                            json_free(breakdown_lines);
                            breakdown_lines = bl;
                        }
                    }
                    json_free(payload);
                }
                json_free(details);
                /* finally */
                json_t *a4 = tmm_args0();
                tmm_aset(a4, "tokens", tokens);
                json_t *cr = tmm_call(ctx, "clear_session_context", a4);
                json_free(tokens);
                if (tmm_is_raise(cr)) {
                    if (inner_err) json_free(inner_err);
                    inner_err = cr;
                } else {
                    json_free(cr);
                }
                if (inner_err) {
                    if (tmm_is_base_exc(tmm_raise_kind(inner_err))) {
                        json_free(live_messages);
                        ret = inner_err;
                        goto cleanup;
                    }
                    json_free(inner_err);
                    json_free(breakdown_lines);
                    breakdown_lines = json_array();
                }
            } else {
                /* outer except Exception: breakdown_lines = [] */
                if (tmm_is_base_exc(tmm_raise_kind(block_err))) {
                    json_free(live_messages);
                    ret = block_err;
                    goto cleanup;
                }
                json_free(block_err);
                json_free(breakdown_lines);
                breakdown_lines = json_array();
            }
            json_free(live_messages);
        }
    }

    /* persisted-state view */
    {
        json_t *usage = tmm_sib_session(ctx, "session_usage_snapshot", session);
        if (tmm_is_raise(usage)) { ret = usage; goto cleanup; }
        json_t *mirror = tmm_sib_session(ctx, "metadata_mirror", session);
        if (tmm_is_raise(mirror)) { json_free(usage); ret = mirror; goto cleanup; }

        char hdr[128];
        if (tmm_truthy(messages)) {
            snprintf(hdr, sizeof hdr, "Conversation: %zu messages", json_len(messages));
        } else {
            snprintf(hdr, sizeof hdr, "Conversation is empty (no messages yet).");
        }
        json_append(lines, json_string(hdr));
        tmm_roles_t roles;
        tmm_count_roles(messages, &roles, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        char roleline[256];
        snprintf(roleline, sizeof roleline,
                 "  user: %lld, assistant: %lld, tool: %lld, system: %lld",
                 roles.user, roles.assistant, roles.tool, roles.system);
        json_append(lines, json_string(roleline));
        const json_t *mm = tmm_get(mirror, "model", NULL, &err);
        if (!err) {
            const json_t *model = mm;
            if (!tmm_truthy(mm)) {
                model = tmm_get(usage, "model", NULL, &err);
            }
            if (!err && tmm_truthy(model)) {
                char *ms = tmm_pystr(model);
                char *ml = malloc(strlen(ms) + 16);
                sprintf(ml, "Model: %s", ms);
                json_append(lines, json_string(ml));
                free(ml); free(ms);
            }
        }
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        const json_t *prov = tmm_get(mirror, "provider", NULL, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        {
            char *ps = tmm_truthy(prov) ? tmm_pystr(prov) : tmm_strdup("auto");
            char *pl = malloc(strlen(ps) + 16);
            sprintf(pl, "Provider: %s", ps);
            json_append(lines, json_string(pl));
            free(pl); free(ps);
        }
        if (tmm_truthy(breakdown_lines)) {
            json_append(lines, json_string(""));
            if (breakdown_lines->type == JSON_ARRAY) {
                for (size_t i = 0; i < breakdown_lines->c.count; i++)
                    json_append(lines, json_copy(breakdown_lines->c.items[i]));
            } else {
                char buf[256];
                snprintf(buf, sizeof buf, "'%s' object is not iterable",
                         tmm_type_name(breakdown_lines));
                json_free(usage); json_free(mirror);
                ret = tmm_err_spec("TypeError", buf);
                goto cleanup;
            }
            json_free(usage); json_free(mirror);
            goto join_lines;
        }
        const json_t *cuv = tmm_get(usage, "context_used", NULL, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        long long context_used = tmm_int_or0(cuv, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        const json_t *est = tmm_get(usage, "context_estimated", NULL, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        const char *mark = tmm_truthy(est) ? "~" : "";
        const json_t *cmv = tmm_get(usage, "context_max", NULL, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        long long context_max = tmm_int_or0(cmv, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        if (context_used && context_max) {
            char *cu = tmm_fmt_comma(context_used);
            char *cm = tmm_fmt_comma(context_max);
            char buf[512];
            snprintf(buf, sizeof buf, "Context usage: %s%s / %s tokens (%s%.1f%%)",
                     mark, cu, cm, mark, (double)context_used / (double)context_max * 100.0);
            json_append(lines, json_string(buf));
            free(cu); free(cm);
        } else if (context_used) {
            char *cu = tmm_fmt_comma(context_used);
            char buf[256];
            snprintf(buf, sizeof buf, "Context usage: %s%s tokens", mark, cu);
            json_append(lines, json_string(buf));
            free(cu);
        }
        const json_t *comp = tmm_get(usage, "compressions", NULL, &err);
        if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
        if (tmm_truthy(comp)) {
            long long cn = tmm_int_or0(comp, &err);
            if (err) { json_free(usage); json_free(mirror); ret = err; goto cleanup; }
            char *cs = tmm_fmt_comma(cn);
            char buf[128];
            snprintf(buf, sizeof buf, "Compressions: %s", cs);
            json_append(lines, json_string(buf));
            free(cs);
        }
        json_free(usage); json_free(mirror);
    }

    /* context-file block (no except — raises propagate after the finally) */
    {
        const json_t *agent = tmm_get(session, "agent", NULL, &err);
        if (err) { ret = err; goto cleanup; }
        if (tmm_agent_present(agent)) {
            const json_t *skey = tmm_sub(session, "session_key", &err);
            if (err) { ret = err; goto cleanup; }
            json_t *cwd = tmm_sib_session(ctx, "session_cwd", session);
            if (tmm_is_raise(cwd)) { ret = cwd; goto cleanup; }
            json_t *a = tmm_args0();
            tmm_asets(a, "session_key", tmm_str(skey));
            tmm_aset(a, "cwd", cwd);
            json_free(cwd);
            json_t *tokens = tmm_call(ctx, "set_session_context", a);
            if (tmm_is_raise(tokens)) { ret = tokens; goto cleanup; }
            json_t *body_err = NULL;
            json_t *file_lines = NULL;
            json_t *a2 = tmm_args0();
            tmm_aset(a2, "agent", tmm_agent_marker(agent));
            json_t *sources = tmm_call(ctx, "context_file_sources_for_agent", a2);
            if (tmm_is_raise(sources)) {
                body_err = sources;
            } else {
                json_t *a3 = tmm_args0();
                tmm_aset(a3, "sources", sources);
                json_free(sources);
                file_lines = tmm_call(ctx, "render_context_file_lines", a3);
                if (tmm_is_raise(file_lines)) { body_err = file_lines; file_lines = NULL; }
            }
            json_t *a4 = tmm_args0();
            tmm_aset(a4, "tokens", tokens);
            json_t *cr = tmm_call(ctx, "clear_session_context", a4);
            json_free(tokens);
            if (tmm_is_raise(cr)) {
                if (body_err) json_free(body_err);
                if (file_lines) json_free(file_lines);
                ret = cr;
                goto cleanup;
            }
            json_free(cr);
            if (body_err) {
                if (file_lines) json_free(file_lines);
                ret = body_err;
                goto cleanup;
            }
            if (tmm_truthy(file_lines)) {
                json_append(lines, json_string(""));
                if (file_lines->type == JSON_ARRAY) {
                    for (size_t i = 0; i < file_lines->c.count; i++)
                        json_append(lines, json_copy(file_lines->c.items[i]));
                } else {
                    char buf[256];
                    snprintf(buf, sizeof buf, "'%s' object is not iterable",
                             tmm_type_name(file_lines));
                    json_free(file_lines);
                    ret = tmm_err_spec("TypeError", buf);
                    goto cleanup;
                }
            }
            json_free(file_lines);
        }
    }

join_lines:
    {
        char *s = tmm_join("\n", lines, &err);
        if (err) { ret = err; goto cleanup; }
        ret = json_string(s);
        free(s);
    }
cleanup:
    json_free(messages);
    json_free(breakdown_lines);
    json_free(lines);
    return ret ? ret : json_null();
}/* ══ _format_live_review_output ══ */
/* PoP: tmm_format_live_review_output @ tui_gateway/methods_slash.py:_format_live_review_output */
static json_t *tmm_format_live_review_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *sid = json_get_str(args, "sid", "");
    const char *arg = json_get_str(args, "arg", "");
    if (!session || session->type == JSON_NULL)
        return json_string("Nothing to review yet \xe2\x80\x94 send a message first.");
    json_t *uses = tmm_sib_session(ctx, "session_uses_compute_host", session);
    if (tmm_is_raise(uses)) return uses;
    bool remote = tmm_truthy(uses);
    json_free(uses);
    if (remote)
        return json_string("/review runs on the local agent only for now \xe2\x80\x94 "
                           "this session's agent lives on a remote compute host.");
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    if (!tmm_agent_present(agent))
        return json_string("Nothing to review yet \xe2\x80\x94 send a message first.");
    const json_t *running = tmm_get(session, "running", NULL, &err);
    if (err) return err;
    if (tmm_truthy(running))
        return json_string("session busy \xe2\x80\x94 wait for the current turn to "
                           "finish, then /review");
    /* with session.get("history_lock") or contextlib.nullcontext(): silent */
    json_t *snapshot = tmm_history_copy(session);
    if (!tmm_truthy(snapshot)) {
        const json_t *sm = tmm_getattr_def(agent, "_session_messages", NULL);
        json_free(snapshot);
        if (tmm_truthy(sm)) {
            if (sm->type != JSON_ARRAY) {
                char buf[256];
                snprintf(buf, sizeof buf, "'%s' object is not iterable",
                         tmm_type_name(sm));
                return tmm_err_spec("TypeError", buf);
            }
            snapshot = json_copy(sm);
        } else {
            snapshot = json_array();
        }
    }
    const json_t *skey = tmm_sub(session, "session_key", &err);
    if (err) { json_free(snapshot); return err; }
    json_t *a = tmm_args0();
    tmm_asets(a, "session_key", tmm_str(skey));
    tmm_asets(a, "ui_session_id", sid);
    json_t *tokens = tmm_call(ctx, "set_session_context", a);
    if (tmm_is_raise(tokens)) { json_free(snapshot); return tokens; }
    json_t *rt = tmm_sib_session(ctx, "runtime_record_set", session);
    if (tmm_is_raise(rt)) { json_free(snapshot); json_free(tokens); return rt; }

    /* try: with _session_profile_runtime_scope(session):
     *          result = start_review(agent, snapshot, arg or "") */
    json_t *body_err = NULL;
    json_t *result = NULL;
    json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
    if (tmm_is_raise(scope)) {
        body_err = scope;
    } else {
        json_free(scope);
        json_t *a2 = tmm_args0();
        tmm_aset(a2, "agent", tmm_agent_marker(agent));
        tmm_aset(a2, "snapshot", snapshot);
        tmm_asets(a2, "arg", arg);
        result = tmm_call(ctx, "start_review", a2);
        if (tmm_is_raise(result)) { body_err = result; result = NULL; }
    }
    json_free(snapshot);
    /* except ValueError / except Exception build the reply BEFORE finally */
    json_t *reply = NULL;
    json_t *propagate = NULL;
    if (body_err) {
        const char *k = tmm_raise_kind(body_err);
        if (tmm_exc_isa(k, "ValueError")) {
            reply = json_string(json_get_str(body_err, "message", ""));
            json_free(body_err);
        } else if (!tmm_is_base_exc(k)) {
            const char *m = json_get_str(body_err, "message", "");
            char *buf = malloc(strlen(m) + 64);
            sprintf(buf, "/review failed to start: %s", m);
            reply = json_string(buf);
            free(buf);
            json_free(body_err);
        } else {
            propagate = body_err;
        }
    }
    /* finally: reset(runtime_token) then _clear_session_context(tokens) */
    json_t *a3 = tmm_args0();
    tmm_aset(a3, "token", rt);
    json_t *reset = tmm_call(ctx, "runtime_record_reset", a3);
    json_free(rt);
    if (tmm_is_raise(reset)) {
        if (reply) json_free(reply);
        if (propagate) json_free(propagate);
        if (result) json_free(result);
        json_free(tokens);
        return reset;
    }
    json_free(reset);
    json_t *a4 = tmm_args0();
    tmm_aset(a4, "tokens", tokens);
    json_t *clear = tmm_call(ctx, "clear_session_context", a4);
    json_free(tokens);
    if (tmm_is_raise(clear)) {
        if (reply) json_free(reply);
        if (propagate) json_free(propagate);
        if (result) json_free(result);
        return clear;
    }
    json_free(clear);
    if (propagate) {
        if (result) json_free(result);
        return propagate;
    }
    if (reply) {
        if (result) json_free(result);
        return reply;
    }
    /* return format_dispatch_note(result, arg or "") — outside the try */
    json_t *a5 = tmm_args0();
    tmm_aset(a5, "result", result);
    tmm_asets(a5, "arg", arg);
    json_free(result);
    return tmm_call(ctx, "format_dispatch_note", a5);
}

/* "skill_manage" in getattr(agent, "valid_tool_names", set()) */
static bool tmm_has_skill_manage(const json_t *agent) {
    const json_t *v = tmm_getattr_def(agent, "valid_tool_names", NULL);
    if (!v) return false;
    if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) {
            const json_t *it = v->c.items[i];
            if (it && it->type == JSON_STRING && !strcmp(it->str_val, "skill_manage"))
                return true;
        }
        return false;
    }
    if (v->type == JSON_STRING)
        return tmm_contains(v->str_val, "skill_manage");
    if (v->type == JSON_OBJECT)
        return json_obj_get(v, "skill_manage") != NULL;
    return false;   /* membership on a scalar raises live; outside the domain */
}

/* ══ _format_live_refine_output ══ */
/* PoP: tmm_format_live_refine_output @ tui_gateway/methods_slash.py:_format_live_refine_output */
static json_t *tmm_format_live_refine_output(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *session = json_obj_get(args, "session");
    const char *sid = json_get_str(args, "sid", "");
    const char *arg = json_get_str(args, "arg", "");
    if (!session || session->type == JSON_NULL)
        return json_string("Refine unavailable (no session).");
    json_t *err = NULL;
    const json_t *running = tmm_get(session, "running", NULL, &err);
    if (err) return err;
    if (tmm_truthy(running))
        return json_string("Agent is running \xe2\x80\x94 wait for the turn to "
                           "finish, then /refine.");
    json_t *uses = tmm_sib_session(ctx, "session_uses_compute_host", session);
    if (tmm_is_raise(uses)) return uses;
    bool remote = tmm_truthy(uses);
    json_free(uses);
    if (remote) {
        char *stripped = tmm_strip(arg);
        tmm_sbuf_t cb; tmm_sb_init(&cb);
        tmm_sb_puts(&cb, "/refine");
        if (stripped[0]) { tmm_sb_putc(&cb, ' '); tmm_sb_puts(&cb, arg); }
        free(stripped);
        char *command = tmm_sb_take(&cb);
        json_t *a = tmm_args0();
        tmm_asets(a, "sid", sid);
        tmm_asets(a, "route_name", "slash.refine");
        tmm_asets(a, "command", command);
        tmm_asetb(a, "wait", true);
        free(command);
        json_t *ack = tmm_call(ctx, "send_compute_host_control", a);
        if (tmm_is_raise(ack)) {
            if (tmm_is_base_exc(tmm_raise_kind(ack))) return ack;
            const char *m = json_get_str(ack, "message", "");
            char *buf = malloc(strlen(m) + 64);
            sprintf(buf, "compute-host slash.refine failed: %s", m);
            json_free(ack);
            json_t *ret = json_string(buf);
            free(buf);
            return ret;
        }
        const json_t *type = tmm_get(ack, "type", NULL, &err);
        if (err) { json_free(ack); return err; }
        const char *ts = tmm_str(type);
        if (ts && (!strcmp(ts, "control.error") || !strcmp(ts, "error"))) {
            const json_t *msg = tmm_get(ack, "message", NULL, &err);
            if (err) { json_free(ack); return err; }
            char *s = tmm_truthy(msg) ? tmm_pystr(msg)
                : tmm_strdup("compute-host slash.refine failed");
            json_free(ack);
            json_t *ret = json_string(s);
            free(s);
            return ret;
        }
        json_t *a2 = tmm_args0();
        tmm_aset(a2, "session", tmm_proj_session(session));
        tmm_aset(a2, "ack", ack);
        json_t *mr = tmm_call(ctx, "apply_compute_host_metadata_mirror", a2);
        if (tmm_is_raise(mr)) { json_free(ack); return mr; }
        json_free(mr);
        const json_t *output = tmm_get(ack, "output", NULL, &err);
        if (err) { json_free(ack); return err; }
        char *s = tmm_str_or_empty(output);
        json_free(ack);
        json_t *ret = json_string(s);
        free(s);
        return ret;
    }
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) return err;
    if (!tmm_agent_present(agent))
        return json_string("Nothing to refine yet \xe2\x80\x94 send a message first.");

    json_t *snapshot = json_array();
    const json_t *key = tmm_get(session, "session_key", NULL, &err);
    if (err) { json_free(snapshot); return err; }
    char *transcript_ref = tmm_str_or_empty(key);
    if (transcript_ref[0]) {
        /* try: with _session_db(session) as db, _persistence_for(db) as p:
         *          if p is not None: snapshot = [m.as_openai_message() ...]
         * except Exception: logger.debug(...) */
        json_t *tx_err = NULL;
        json_t *db = tmm_sib_session(ctx, "session_db", session);
        if (tmm_is_raise(db)) {
            tx_err = db;
        } else {
            json_t *a = tmm_args0();
            tmm_aset(a, "db", db);
            json_t *p = tmm_call(ctx, "persistence_for", a);
            json_free(db);
            if (tmm_is_raise(p)) {
                tx_err = p;
            } else {
                if (tmm_agent_present(p)) {
                    json_t *a2 = tmm_args0();
                    tmm_asets(a2, "transcript_ref", transcript_ref);
                    tmm_asetb(a2, "include_lineage", true);
                    json_t *msgs = tmm_call(ctx, "persistence.read_conversation", a2);
                    if (tmm_is_raise(msgs)) {
                        tx_err = msgs;
                    } else if (!msgs || msgs->type != JSON_ARRAY) {
                        char buf[256];
                        snprintf(buf, sizeof buf, "'%s' object is not iterable",
                                 tmm_type_name(msgs));
                        json_free(msgs);
                        tx_err = tmm_err_spec("TypeError", buf);
                    } else {
                        /* the comprehension assigns snapshot only on full
                         * success — a mid-read raise leaves snapshot [] */
                        json_t *collected = json_array();
                        for (size_t i = 0; i < msgs->c.count; i++) {
                            json_t *m = tmm_call(ctx, "message.as_openai_message",
                                                 tmm_args0());
                            if (tmm_is_raise(m)) { tx_err = m; break; }
                            json_append(collected, m);
                        }
                        json_free(msgs);
                        if (!tx_err) {
                            json_free(snapshot);
                            snapshot = collected;
                        } else {
                            json_free(collected);
                        }
                    }
                }
                json_free(p);
            }
        }
        if (tx_err) {
            if (tmm_is_base_exc(tmm_raise_kind(tx_err))) {
                free(transcript_ref);
                json_free(snapshot);
                return tx_err;
            }
            json_free(tx_err);
            json_t *a3 = tmm_args0();
            tmm_asets(a3, "msg", "failed to load persisted /refine transcript");
            tmm_asetb(a3, "exc_info", true);
            json_t *lg = tmm_call(ctx, "logger.debug", a3);
            if (tmm_is_raise(lg)) {
                free(transcript_ref);
                json_free(snapshot);
                return lg;
            }
            json_free(lg);
        }
    }
    free(transcript_ref);
    if (!tmm_truthy(snapshot)) {
        json_t *lock_err = tmm_history_lock(session);
        if (lock_err) { json_free(snapshot); return lock_err; }
        json_free(snapshot);
        snapshot = tmm_history_copy(session);
    }
    if (!tmm_truthy(snapshot)) {
        json_free(snapshot);
        return json_string("Nothing to refine yet \xe2\x80\x94 the conversation is empty.");
    }
    bool review_skills = tmm_has_skill_manage(agent);
    char *focus = tmm_strip(arg);
    json_t *a = tmm_args0();
    tmm_aset(a, "messages_snapshot", snapshot);
    tmm_asetb(a, "review_memory", true);
    tmm_asetb(a, "review_skills", review_skills);
    tmm_asets(a, "focus", focus[0] ? focus : NULL);
    tmm_asetb(a, "explicit", true);
    json_t *r = tmm_call(ctx, "agent.spawn_background_review", a);
    if (tmm_is_raise(r)) {
        free(focus);
        json_free(snapshot);
        if (tmm_is_base_exc(tmm_raise_kind(r))) return r;
        const char *m = json_get_str(r, "message", "");
        char *buf = malloc(strlen(m) + 64);
        sprintf(buf, "/refine failed to start: %s", m);
        json_free(r);
        json_t *ret = json_string(buf);
        free(buf);
        return ret;
    }
    json_free(r);
    json_free(snapshot);
    tmm_sbuf_t sb; tmm_sb_init(&sb);
    tmm_sb_puts(&sb, "\xe2\x9a\x97 Reviewing this conversation in the background");
    if (focus[0]) {
        tmm_sb_puts(&sb, " (focus: ");
        tmm_sb_puts(&sb, focus);
        tmm_sb_puts(&sb, ")");
    }
    free(focus);
    tmm_sb_puts(&sb, " \xe2\x80\x94 any memory/skill updates will be reported when done.");
    char *s = tmm_sb_take(&sb);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}/* estimate(messages, prompt, tool_defs): the closure calls the seam only
 * when messages is truthy, else returns 0. */
static json_t *tmm_estimate(tmm_ctx_t *ctx, const json_t *messages,
                            const char *prompt, const json_t *tools) {
    if (!tmm_truthy(messages)) return json_number(0);
    json_t *a = tmm_args0();
    tmm_aset(a, "messages", messages);
    tmm_asets(a, "system_prompt", prompt);
    tmm_aset(a, "tools", tools);
    return tmm_call(ctx, "estimate_request_tokens_rough", a);
}

/* ══ _compress_live_with_feedback ══ */
/* PoP: tmm_compress_live_with_feedback @ tui_gateway/methods_slash.py:_compress_live_with_feedback */
static json_t *tmm_compress_live_with_feedback(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const json_t *agent = json_obj_get(args, "agent");
    const char *arg = json_get_str(args, "arg", "");
    bool snapshot_kwargs = json_get_bool(args, "snapshot_kwargs", false);
    json_t *err = NULL;

    json_t *a = tmm_args0();
    tmm_asets(a, "arg", arg);
    json_t *request = tmm_call(ctx, "parse_compress_args", a);
    if (tmm_is_raise(request)) return request;
    const json_t *aggressive = tmm_attr_req(request, "aggressive", &err);
    if (err) { json_free(request); return err; }
    if (tmm_truthy(aggressive)) {
        json_free(request);
        return json_string(TMM_AGGRESSIVE_UNSUPPORTED);
    }
    const json_t *preview = tmm_attr_req(request, "preview", &err);
    if (err) { json_free(request); return err; }
    if (tmm_truthy(preview)) {
        json_t *lock_err = tmm_history_lock(session);
        if (lock_err) { json_free(request); return lock_err; }
        json_t *preview_messages = tmm_history_copy(session);
        json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
        if (tmm_is_raise(scope)) {
            json_free(preview_messages); json_free(request); return scope;
        }
        json_free(scope);
        json_t *a2 = tmm_args0();
        tmm_aset(a2, "agent", tmm_agent_marker(agent));
        tmm_aset(a2, "preview_messages", preview_messages);
        tmm_aset(a2, "request", request);
        json_free(preview_messages);
        json_t *r = tmm_call(ctx, "compress_now", a2);
        if (tmm_is_raise(r)) { json_free(request); return r; }
        json_t *a3 = tmm_args0();
        tmm_aset(a3, "result", r);
        json_free(r);
        json_t *lines = tmm_call(ctx, "render_compress_result", a3);
        json_free(request);
        if (tmm_is_raise(lines)) return lines;
        char *s = tmm_join("\n", lines, &err);
        json_free(lines);
        if (err) return err;
        json_t *ret = json_string(s);
        free(s);
        return ret;
    }

    json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
    if (tmm_is_raise(scope)) { json_free(request); return scope; }
    json_free(scope);
    json_t *a4 = tmm_args0();
    tmm_asets(a4, "sid", sid);
    tmm_aset(a4, "session", tmm_proj_session(session));
    json_t *turn = tmm_call(ctx, "manual_compress_turn", a4);
    if (tmm_is_raise(turn)) { json_free(request); return turn; }
    json_free(turn);
    json_t *lock_err = tmm_history_lock(session);
    if (lock_err) { json_free(request); return lock_err; }
    json_t *before_messages = tmm_history_copy(session);
    json_t *zero = json_number(0);
    const json_t *hv = tmm_get(session, "history_version", zero, &err);
    if (err) { json_free(zero); json_free(before_messages); json_free(request); return err; }
    long long history_version = tmm_int(hv, &err);
    json_free(zero);
    if (err) { json_free(before_messages); json_free(request); return err; }
    char *sys_prompt = tmm_str_or_empty(tmm_getattr_def(agent, "_cached_system_prompt", NULL));
    const json_t *tools_node = tmm_getattr_def(agent, "tools", NULL);
    const json_t *tools = tmm_truthy(tools_node) ? tools_node : NULL;
    json_t *before_tokens = tmm_estimate(ctx, before_messages, sys_prompt, tools);
    if (tmm_is_raise(before_tokens)) {
        free(sys_prompt); json_free(before_messages); json_free(request);
        return before_tokens;
    }

    /* try: _compress_session_history(session, arg.strip() or None, **snapshot)
     *      or _compress_session_history(session, arg)
     * except CompressionLockHeld as e: describe_compression_lock_skip(e.holder) */
    json_t *a5 = tmm_args0();
    tmm_aset(a5, "session", tmm_proj_session(session));
    if (snapshot_kwargs) {
        char *stripped = tmm_strip(arg);
        tmm_asets(a5, "arg", stripped[0] ? stripped : NULL);
        free(stripped);
        tmm_aset(a5, "approx_tokens", before_tokens);
        tmm_aset(a5, "before_messages", before_messages);
        tmm_asetn(a5, "history_version", (double)history_version);
    } else {
        tmm_asets(a5, "arg", arg);
    }
    json_t *cr = tmm_call(ctx, "compress_session_history", a5);
    if (tmm_is_raise(cr)) {
        if (tmm_exc_isa(tmm_raise_kind(cr), "CompressionLockHeld")) {
            json_t *a6 = tmm_args0();
            tmm_aset(a6, "holder", json_obj_get(cr, "holder"));
            json_free(cr);
            json_t *d = tmm_call(ctx, "describe_compression_lock_skip", a6);
            free(sys_prompt); json_free(before_messages);
            json_free(before_tokens); json_free(request);
            return d;
        }
        free(sys_prompt); json_free(before_messages);
        json_free(before_tokens); json_free(request);
        return cr;
    }
    json_free(cr);
    json_t *a_sync = tmm_args0();
    tmm_asets(a_sync, "sid", sid);
    tmm_aset(a_sync, "session", tmm_proj_session(session));
    json_t *sync = tmm_call(ctx, "sync_session_key_after_compress", a_sync);
    if (tmm_is_raise(sync)) {
        free(sys_prompt); json_free(before_messages);
        json_free(before_tokens); json_free(request);
        return sync;
    }
    json_free(sync);
    lock_err = tmm_history_lock(session);
    if (lock_err) {
        free(sys_prompt); json_free(before_messages);
        json_free(before_tokens); json_free(request);
        return lock_err;
    }
    json_t *after_messages = tmm_history_copy(session);
    const json_t *asp_node = tmm_getattr_def(agent, "_cached_system_prompt", NULL);
    char *after_prompt = tmm_truthy(asp_node) ? tmm_pystr(asp_node) : tmm_strdup(sys_prompt);
    const json_t *at_node = tmm_getattr_def(agent, "tools", NULL);
    const json_t *after_tools = tmm_truthy(at_node) ? at_node : tools;
    json_t *after_tokens = tmm_estimate(ctx, after_messages, after_prompt, after_tools);
    free(after_prompt);
    if (tmm_is_raise(after_tokens)) {
        free(sys_prompt); json_free(before_messages); json_free(after_messages);
        json_free(before_tokens); json_free(request);
        return after_tokens;
    }
    json_t *a7 = tmm_args0();
    tmm_aset(a7, "agent", tmm_agent_marker(agent));
    tmm_aset(a7, "session", tmm_proj_session(session));
    json_t *info = tmm_call(ctx, "session_info", a7);
    if (tmm_is_raise(info)) {
        free(sys_prompt); json_free(before_messages); json_free(after_messages);
        json_free(before_tokens); json_free(after_tokens); json_free(request);
        return info;
    }
    json_t *a8 = tmm_args0();
    tmm_asets(a8, "event", "session.info");
    tmm_asets(a8, "sid", sid);
    tmm_aset(a8, "payload", info);
    json_free(info);
    json_t *em = tmm_call(ctx, "emit", a8);
    if (tmm_is_raise(em)) {
        free(sys_prompt); json_free(before_messages); json_free(after_messages);
        json_free(before_tokens); json_free(after_tokens); json_free(request);
        return em;
    }
    json_free(em);
    json_t *a9 = tmm_args0();
    tmm_aset(a9, "before_messages", before_messages);
    tmm_aset(a9, "after_messages", after_messages);
    tmm_aset(a9, "before_tokens", before_tokens);
    tmm_aset(a9, "after_tokens", after_tokens);
    tmm_aset(a9, "compression_state", tmm_getattr_def(agent, "context_compressor", NULL));
    json_t *fb = tmm_call(ctx, "summarize_manual_compression", a9);
    if (tmm_is_raise(fb)) {
        free(sys_prompt); json_free(before_messages); json_free(after_messages);
        json_free(before_tokens); json_free(after_tokens); json_free(request);
        return fb;
    }
    /* finalize runs BEFORE the return expression's fb[...] subscripts */
    json_t *a10 = tmm_args0();
    tmm_aset(a10, "agent", tmm_agent_marker(agent));
    tmm_asetb(a10, "committed", true);
    json_t *fin = tmm_call(ctx, "finalize_context_engine_notification", a10);
    if (tmm_is_raise(fin)) {
        json_free(fb);
        free(sys_prompt); json_free(before_messages); json_free(after_messages);
        json_free(before_tokens); json_free(after_tokens); json_free(request);
        return fin;
    }
    json_free(fin);
    const json_t *headline = tmm_sub(fb, "headline", &err);
    if (!err) {
        const json_t *token_line = tmm_sub(fb, "token_line", &err);
        if (!err) {
            const json_t *note = tmm_get(fb, "note", NULL, &err);
            if (!err) {
                json_t *parts = json_array();
                if (tmm_truthy(headline)) json_append(parts, json_copy(headline));
                if (tmm_truthy(token_line)) json_append(parts, json_copy(token_line));
                if (tmm_truthy(note)) json_append(parts, json_copy(note));
                char *s = tmm_join("\n", parts, &err);
                json_free(parts);
                json_free(fb);
                free(sys_prompt); json_free(before_messages); json_free(after_messages);
                json_free(before_tokens); json_free(after_tokens); json_free(request);
                if (err) return err;
                json_t *ret = json_string(s);
                free(s);
                return ret;
            }
        }
    }
    json_free(fb);
    free(sys_prompt); json_free(before_messages); json_free(after_messages);
    json_free(before_tokens); json_free(after_tokens); json_free(request);
    return err;
}

/* ══ _mirror_approvals ══ */
/* PoP: tmm_mirror_approvals @ tui_gateway/methods_slash.py:_mirror_approvals */
static json_t *tmm_mirror_approvals(tmm_ctx_t *ctx, const json_t *args) {
    const char *arg = json_get_str(args, "arg", "");
    if (arg && arg[0]) {
        json_t *r = tmm_call(ctx, "broadcast_session_info", tmm_args0());
        if (tmm_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ══ _mirror_personality ══ */
/* PoP: tmm_mirror_personality @ tui_gateway/methods_slash.py:_mirror_personality */
static json_t *tmm_mirror_personality(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const json_t *agent = json_obj_get(args, "agent");
    const char *arg = json_get_str(args, "arg", "");
    if (!(arg && arg[0]) || !tmm_agent_present(agent)) return json_null();
    json_t *cfg = tmm_call(ctx, "load_cfg", tmm_args0());
    if (tmm_is_raise(cfg)) return cfg;
    json_t *a = tmm_args0();
    tmm_asets(a, "arg", arg);
    tmm_aset(a, "cfg", cfg);
    json_free(cfg);
    json_t *v = tmm_call(ctx, "validate_personality", a);
    if (tmm_is_raise(v)) return v;
    json_t *err = NULL;
    const json_t *pname = NULL, *new_prompt = NULL;
    tmm_unpack2(v, &pname, &new_prompt, &err);
    if (err) { json_free(v); return err; }
    json_t *a2 = tmm_args0();
    tmm_asets(a2, "pname", tmm_str(pname));
    json_t *r = tmm_call(ctx, "persist_personality", a2);
    if (tmm_is_raise(r)) { json_free(v); return r; }
    json_free(r);
    json_t *a3 = tmm_args0();
    tmm_asets(a3, "sid", sid);
    tmm_aset(a3, "session", tmm_proj_session(session));
    tmm_aset(a3, "new_prompt", new_prompt);
    tmm_aset(a3, "pname", pname);
    json_t *r2 = tmm_call(ctx, "apply_personality_to_session", a3);
    json_free(v);
    if (tmm_is_raise(r2)) return r2;
    json_free(r2);
    return json_null();
}

/* ══ _mirror_fast ══ */
/* PoP: tmm_mirror_fast @ tui_gateway/methods_slash.py:_mirror_fast */
static json_t *tmm_mirror_fast(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const json_t *agent = json_obj_get(args, "agent");
    const char *arg = json_get_str(args, "arg", "");
    if (!tmm_agent_present(agent)) return json_null();
    char *lower = tmm_lower(arg);
    const char *tier = tmm_fast_tier(lower);
    free(lower);
    /* agent.service_tier = _FAST_TIERS[arg.lower()] when the arg is a tier;
     * the state mirror reads None when no set happened */
    json_set(ctx->state, "service_tier",
             (tier && tier[0]) ? json_string(tier) : json_null());
    json_t *a = tmm_args0();
    tmm_aset(a, "agent", tmm_agent_marker(agent));
    tmm_aset(a, "session", tmm_proj_session(session));
    json_t *info = tmm_call(ctx, "session_info", a);
    if (tmm_is_raise(info)) return info;
    json_t *a2 = tmm_args0();
    tmm_asets(a2, "event", "session.info");
    tmm_asets(a2, "sid", sid);
    tmm_aset(a2, "payload", info);
    json_free(info);
    json_t *em = tmm_call(ctx, "emit", a2);
    if (tmm_is_raise(em)) return em;
    json_free(em);
    return json_null();
}

/* ══ _mirror_reload_mcp ══ */
/* PoP: tmm_mirror_reload_mcp @ tui_gateway/methods_slash.py:_mirror_reload_mcp */
static json_t *tmm_mirror_reload_mcp(tmm_ctx_t *ctx, const json_t *args) {
    const json_t *agent = json_obj_get(args, "agent");
    if (tmm_agent_present(agent) && json_has(agent, "reload_mcp_tools")) {
        json_t *r = tmm_call(ctx, "agent.reload_mcp_tools", tmm_args0());
        if (tmm_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ══ _mirror_stop ══ */
/* PoP: tmm_mirror_stop @ tui_gateway/methods_slash.py:_mirror_stop */
static json_t *tmm_mirror_stop(tmm_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *a = tmm_args0();
    tmm_asets(a, "source", "slash.stop");
    json_t *r = tmm_call(ctx, "process_registry.kill_all", a);
    if (tmm_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* ══ _mirror_autoprune ══ */
/* PoP: tmm_mirror_autoprune @ tui_gateway/methods_slash.py:_mirror_autoprune */
static json_t *tmm_mirror_autoprune(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const char *arg = json_get_str(args, "arg", "");
    json_t *err = NULL;
    const json_t *home = tmm_get(session, "profile_home", NULL, &err);
    if (err) return err;
    if (arg && arg[0]) {
        json_t *a = tmm_args0();
        tmm_aset(a, "home", tmm_truthy(home) ? home : NULL);
        json_t *answered = tmm_call(ctx, "prune_advisor.answered", a);
        if (tmm_is_raise(answered)) return answered;
        bool yes = tmm_truthy(answered);
        json_free(answered);
        if (yes) {
            json_t *payload = json_object();
            json_set_hdr(payload, "key", "sessions.auto_prune");
            json_t *a2 = tmm_args0();
            tmm_asets(a2, "event", "notification.clear");
            tmm_asets(a2, "sid", sid);
            tmm_aset(a2, "payload", payload);
            json_free(payload);
            json_t *em = tmm_call(ctx, "emit", a2);
            if (tmm_is_raise(em)) return em;
            json_free(em);
        }
    }
    return json_string("");
}/* ══ _compute_host_slash ══ */
/* PoP: tmm_compute_host_slash @ tui_gateway/methods_slash.py:_compute_host_slash */
static json_t *tmm_compute_host_slash(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const char *name = json_get_str(args, "name", "");
    const char *command = json_get_str(args, "command", "");
    char *route_name = malloc(strlen(name) + 16);
    sprintf(route_name, "slash.%s", name);
    bool is_compress = !strcmp(name, "compress");
    json_t *ack;
    if (!strcmp(name, "personality")) {
        json_t *a = tmm_args0();
        tmm_asets(a, "sid", sid);
        tmm_asets(a, "route_name", route_name);
        tmm_asets(a, "command", command);
        tmm_asetb(a, "wait", true);
        ack = tmm_call(ctx, "send_compute_host_control", a);
    } else {
        json_t *a = tmm_args0();
        tmm_asets(a, "sid", sid);
        tmm_aset(a, "session", tmm_proj_session(session));
        tmm_asets(a, "route_name", route_name);
        tmm_asets(a, "command", command);
        tmm_asetb(a, "compress", is_compress);
        ack = tmm_call(ctx, "send_claimed_compute_host_control", a);
    }
    if (tmm_is_raise(ack)) {
        const char *k = tmm_raise_kind(ack);
        json_t *ret = NULL;
        if (tmm_exc_isa(k, "queue.Empty")) {
            ret = json_array();
            if (is_compress) {
                json_append(ret, json_string("pending"));
                json_append(ret, json_string(
                    "compression still running in the background; the transcript "
                    "will refresh when it finishes"));
            } else {
                json_append(ret, json_string("failed"));
                char *buf = malloc(strlen(route_name) + 64);
                sprintf(buf, "compute-host %s failed: timed out", route_name);
                json_append(ret, json_string(buf));
                free(buf);
            }
            json_free(ack);
            free(route_name);
            return ret;
        }
        if (tmm_exc_isa(k, "SessionNotOwned")) {
            free(route_name);
            return ack;   /* refused before the host was contacted: re-raise */
        }
        if (tmm_is_base_exc(k)) {
            free(route_name);
            return ack;
        }
        const char *m = json_get_str(ack, "message", "");
        char *buf = malloc(strlen(m) + strlen(route_name) + 64);
        sprintf(buf, "compute-host %s failed: %s", route_name, m);
        json_free(ack);
        free(route_name);
        ret = json_array();
        json_append(ret, json_string("failed"));
        json_append(ret, json_string(buf));
        free(buf);
        return ret;
    }
    json_t *err = NULL;
    const json_t *type = tmm_get(ack, "type", NULL, &err);
    if (err) { json_free(ack); free(route_name); return err; }
    const char *ts = tmm_str(type);
    if (ts && (!strcmp(ts, "control.error") || !strcmp(ts, "error"))) {
        const json_t *msg = tmm_get(ack, "message", NULL, &err);
        if (err) { json_free(ack); free(route_name); return err; }
        char *s;
        if (tmm_truthy(msg)) {
            s = tmm_pystr(msg);
        } else {
            s = malloc(strlen(route_name) + 64);
            sprintf(s, "compute-host %s failed", route_name);
        }
        json_free(ack);
        free(route_name);
        json_t *ret = json_array();
        json_append(ret, json_string("rejected"));
        json_append(ret, json_string(s));
        free(s);
        return ret;
    }
    json_t *a2 = tmm_args0();
    tmm_aset(a2, "session", tmm_proj_session(session));
    tmm_aset(a2, "ack", ack);
    json_t *mr = tmm_call(ctx, "apply_compute_host_metadata_mirror", a2);
    if (tmm_is_raise(mr)) { json_free(ack); free(route_name); return mr; }
    json_free(mr);
    const json_t *output = tmm_get(ack, "output", NULL, &err);
    if (err) { json_free(ack); free(route_name); return err; }
    char *s = tmm_str_or_empty(output);
    json_free(ack);
    free(route_name);
    json_t *ret = json_array();
    json_append(ret, json_string("ok"));
    json_append(ret, json_string(s));
    free(s);
    return ret;
}

/* command.lstrip("/").split(None, 1): name + stripped remainder. Returns
 * false when there is no first token. */
static bool tmm_split_command(const char *command, char **name, char **arg) {
    const char *p = command ? command : "";
    while (*p == '/') p++;
    while (tmm_is_py_ws(*p)) p++;
    if (!*p) { *name = NULL; *arg = NULL; return false; }
    const char *start = p;
    while (*p && !tmm_is_py_ws(*p)) p++;
    *name = malloc((size_t)(p - start) + 1);
    memcpy(*name, start, (size_t)(p - start));
    (*name)[p - start] = '\0';
    while (tmm_is_py_ws(*p)) p++;
    *arg = tmm_strip(p);
    return true;
}

/* ══ _mirror_slash_side_effects ══ */
/* PoP: tmm_mirror_slash_side_effects @ tui_gateway/methods_slash.py:_mirror_slash_side_effects */
static json_t *tmm_mirror_slash_side_effects(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const char *command = json_get_str(args, "command", "");
    char *name = NULL, *arg = NULL;
    if (!tmm_split_command(command, &name, &arg)) return json_string("");
    if (!strcmp(name, "compact")) {
        free(name);
        name = tmm_strdup("compress");
    }
    json_t *err = NULL;
    const json_t *agent = tmm_get(session, "agent", NULL, &err);
    if (err) { free(name); free(arg); return err; }

    if (tmm_in_list(TMM_MUTATES_RUNNING, name)) {
        json_t *uses = tmm_sib_session(ctx, "session_uses_compute_host", session);
        if (tmm_is_raise(uses)) { free(name); free(arg); return uses; }
        bool remote = tmm_truthy(uses);
        json_free(uses);
        if (remote) {
            json_t *a = tmm_args0();
            tmm_asets(a, "sid", sid);
            tmm_aset(a, "session", tmm_proj_session(session));
            tmm_asets(a, "name", name);
            tmm_asets(a, "command", command);
            json_t *r = tmm_call(ctx, "compute_host_slash", a);
            free(name); free(arg);
            if (tmm_is_raise(r)) return r;
            const json_t *text = tmm_index1(r, &err);
            if (err) { json_free(r); return err; }
            json_t *ret = json_copy(text);
            json_free(r);
            return ret;
        }
        const json_t *running = tmm_get(session, "running", NULL, &err);
        if (err) { free(name); free(arg); return err; }
        if (tmm_truthy(running)) {
            const json_t *mca = tmm_get(session, "_manual_compress_active", NULL, &err);
            if (err) { free(name); free(arg); return err; }
            json_t *a = tmm_args0();
            tmm_asets(a, "command", name);
            tmm_asetb(a, "compressing", tmm_truthy(mca));
            json_t *r = tmm_call(ctx, "busy_message", a);
            free(name); free(arg);
            return r;
        }
    }

    /* _SLASH_MIRRORS dispatch */
    const char *mirror_seam = NULL;
    bool is_model = !strcmp(name, "model");
    bool is_compress_mirror = !strcmp(name, "compress");
    if (is_model) mirror_seam = "model";
    else if (!strcmp(name, "approvals")) mirror_seam = "mirror_approvals";
    else if (!strcmp(name, "personality")) mirror_seam = "mirror_personality";
    else if (is_compress_mirror) mirror_seam = "compress_live_with_feedback";
    else if (!strcmp(name, "fast")) mirror_seam = "mirror_fast";
    else if (!strcmp(name, "reload-mcp")) mirror_seam = "mirror_reload_mcp";
    else if (!strcmp(name, "stop")) mirror_seam = "mirror_stop";
    else if (!strcmp(name, "autoprune")) mirror_seam = "mirror_autoprune";
    if (!mirror_seam) { free(name); free(arg); return json_string(""); }

    /* try: with _session_profile_runtime_scope(session):
     *          return mirror(sid, session, agent, arg) or "" */
    json_t *body_err = NULL;
    json_t *result = NULL;
    json_t *scope = tmm_sib_session(ctx, "profile_runtime_scope", session);
    if (tmm_is_raise(scope)) {
        body_err = scope;
    } else {
        json_free(scope);
        if (is_model) {
            /* lambda: _apply_model_switch(sid, session, arg).get("warning", "")
             *         if arg and agent else "" */
            if (arg[0] && tmm_agent_present(agent)) {
                json_t *a = tmm_args0();
                tmm_asets(a, "sid", sid);
                tmm_aset(a, "session", tmm_proj_session(session));
                tmm_asets(a, "arg", arg);
                json_t *r = tmm_call(ctx, "apply_model_switch", a);
                if (tmm_is_raise(r)) {
                    body_err = r;
                } else {
                    json_t *def = json_string("");
                    const json_t *warning = tmm_get(r, "warning", def, &err);
                    if (err) {
                        json_free(def); json_free(r);
                        body_err = err;
                    } else {
                        result = tmm_truthy(warning) ? json_copy(warning) : json_string("");
                        json_free(def);
                        json_free(r);
                    }
                }
            } else {
                result = json_string("");
            }
        } else if (is_compress_mirror) {
            /* lambda: _compress_live_with_feedback(sid, session, agent, arg,
             *           snapshot_kwargs=False) if agent else "" */
            if (tmm_agent_present(agent)) {
                json_t *a = tmm_args0();
                tmm_asets(a, "sid", sid);
                tmm_aset(a, "session", tmm_proj_session(session));
                tmm_aset(a, "agent", tmm_agent_marker(agent));
                tmm_asets(a, "arg", arg);
                tmm_asetb(a, "snapshot_kwargs", false);
                result = tmm_call(ctx, "compress_live_with_feedback", a);
                if (tmm_is_raise(result)) { body_err = result; result = NULL; }
            } else {
                result = json_string("");
            }
        } else {
            result = tmm_sib_mirror4(ctx, mirror_seam, sid, session, agent, arg);
            if (tmm_is_raise(result)) { body_err = result; result = NULL; }
        }
    }
    if (!body_err) {
        /* mirror(...) or "" */
        json_t *ret = (result && tmm_truthy(result)) ? result : json_string("");
        if (result && !tmm_truthy(result)) json_free(result);
        free(name); free(arg);
        return ret;
    }
    if (result) json_free(result);
    const char *k = tmm_raise_kind(body_err);
    if (tmm_exc_isa(k, "CompressionBusy")) {
        json_t *ret = json_string(json_get_str(body_err, "message", ""));
        json_free(body_err);
        free(name); free(arg);
        return ret;
    }
    if (tmm_exc_isa(k, "SessionNotOwned") || tmm_is_base_exc(k)) {
        free(name); free(arg);
        return body_err;
    }
    /* except Exception: finalize a failed compress, then the sync warning */
    if (is_compress_mirror && tmm_agent_present(agent)) {
        json_t *a = tmm_args0();
        tmm_aset(a, "agent", tmm_agent_marker(agent));
        tmm_asetb(a, "committed", false);
        json_t *fin = tmm_call(ctx, "finalize_context_engine_notification", a);
        if (tmm_is_raise(fin)) {
            json_free(body_err);
            free(name); free(arg);
            return fin;
        }
        json_free(fin);
    }
    const char *m = json_get_str(body_err, "message", "");
    char *buf = malloc(strlen(m) + 64);
    sprintf(buf, "live session sync failed: %s", m);
    json_free(body_err);
    free(name); free(arg);
    json_t *ret = json_string(buf);
    free(buf);
    return ret;
}

/* ══ _live_slash_command_output ══ */
/* PoP: tmm_live_slash_command_output @ tui_gateway/methods_slash.py:_live_slash_command_output */
static json_t *tmm_live_slash_command_output(tmm_ctx_t *ctx, const json_t *args) {
    const char *sid = json_get_str(args, "sid", "");
    const json_t *session = json_obj_get(args, "session");
    const char *raw_name = json_get_str(args, "name", "");
    const char *arg = json_get_str(args, "arg", "");
    char *ls = tmm_lstrip(raw_name, '/');
    char *name = tmm_lower(ls);
    free(ls);
    bool session_present = session && session->type != JSON_NULL;

    if (!strcmp(name, "model")) {
        char *stripped = tmm_strip(arg);
        bool bare = !stripped[0];
        free(stripped);
        if (bare) {
            /* _format_live_model_output(session or {}) */
            json_t *empty = json_object();
            const json_t *sess = tmm_truthy(session) ? session : empty;
            json_t *r = tmm_sib_session(ctx, "format_live_model_output", sess);
            if (sess == empty) json_free(empty);
            free(name);
            return r;
        }
    }
    if (tmm_in_list(TMM_CONTEXT_LOCAL, name) && session_present) {
        json_t *uses = tmm_sib_session(ctx, "session_uses_compute_host", session);
        if (tmm_is_raise(uses)) { free(name); return uses; }
        bool remote = tmm_truthy(uses);
        json_free(uses);
        if (!remote) {
            json_t *r = tmm_sib_fmt3(ctx, "format_live_context_output", sid, session, arg);
            free(name);
            return r;
        }
    }
    if (tmm_in_list(TMM_ISOLATED_READ, name)) {
        bool remote = false;
        if (session_present) {
            json_t *uses = tmm_sib_session(ctx, "session_uses_compute_host", session);
            if (tmm_is_raise(uses)) { free(name); return uses; }
            remote = tmm_truthy(uses);
            json_free(uses);
        }
        if (!remote) { free(name); return json_null(); }
    }

    /* _LIVE_SLASH_OUTPUT entry table */
    const char *no_session = NULL;   /* NULL == the None no-session reply */
    const char *fixed = NULL;
    const char *fmt_seam = NULL;
    bool compress_lambda = false;
    if (!strcmp(name, "compress")) {
        no_session = "no active session for /compress";
        compress_lambda = true;
    } else if (!strcmp(name, "usage")) {
        no_session = TMM_NO_AGENT_USAGE;
        fmt_seam = "format_live_usage_output";
    } else if (!strcmp(name, "review")) {
        fmt_seam = "format_live_review_output";
    } else if (!strcmp(name, "history")) {
        no_session = "No conversation history yet.";
        fmt_seam = "format_live_history_output";
    } else if (!strcmp(name, "refine")) {
        fmt_seam = "format_live_refine_output";
    } else if (!strcmp(name, "status")) {
        fmt_seam = "format_live_status_output";
    } else if (!strcmp(name, "context")) {
        no_session = "Conversation is empty (no messages yet).";
        fmt_seam = "format_live_context_output";
    } else if (!strcmp(name, "tools")) {
        no_session = "No tools available.";
        fmt_seam = "format_live_tools_output";
    } else if (!strcmp(name, "help")) {
        fmt_seam = "format_live_help_output";
    } else if (!strcmp(name, "clear")) {
        fixed = "Screen clear is terminal-only; desktop/TUI chat left unchanged.";
    } else if (!strcmp(name, "models")) {
        fixed = "Use /model to view or switch the current model; desktop users "
                "can also open the model picker.";
    } else if (!strcmp(name, "rename")) {
        fixed = "Use /title <name> to rename this session.";
    } else if (!strcmp(name, "effort")) {
        fixed = "Use /reasoning <effort> to change reasoning effort.";
    } else {
        free(name);
        return json_null();
    }
    if (!session_present && no_session) {
        json_t *ret = json_string(no_session);
        free(name);
        return ret;
    }
    if (fixed) {
        json_t *ret = json_string(fixed);
        free(name);
        return ret;
    }
    json_t *ret;
    if (compress_lambda) {
        /* f"/compress {arg}".strip() */
        char *cmd = malloc(strlen(arg) + 16);
        sprintf(cmd, "/compress %s", arg);
        char *stripped = tmm_strip(cmd);
        free(cmd);
        json_t *a = tmm_args0();
        tmm_asets(a, "sid", sid);
        tmm_aset(a, "session", tmm_proj_session(session));
        tmm_asets(a, "command", stripped);
        free(stripped);
        ret = tmm_call(ctx, "mirror_slash_side_effects", a);
    } else {
        ret = tmm_sib_fmt3(ctx, fmt_seam, sid, session, arg);
    }
    free(name);
    return ret;
}