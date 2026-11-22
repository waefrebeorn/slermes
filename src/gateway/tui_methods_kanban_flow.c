/*
 * tui_methods_kanban_flow.c — FLOW + leaf port of the TUI kanban method
 * surface (hermes-agent).
 *
 * MODULE RESOLUTION: the task named tui_gateway/methods_kanban.py, which does
 * NOT exist in the quarry. The TUI's only kanban RPC verb is "kanban.run"
 * (tui_gateway/contracts/canonical.py), dispatched by
 * gateway/session_controls.py:kanban_run to the engine module
 * gateway/session_kanban.py. Those are the REAL defining modules; every PoP
 * below credits them (the scanner only credits the defining module).
 *
 * Ports the DECISION SEQUENCES — branch order, refusal composition, seam call
 * order, module-global state transitions and exception paths — of:
 *   gateway/session_kanban.py: build_kanban_policy, run_task, admit_claim
 *     (run_task closure), bind_worker_context, _watch_submitter, watch
 *     (_watch_submitter closure), run_worker_turns, _worker_workspace,
 *     _run_worker_turns, record_start_failure, _record_worker_result,
 *     record_worker_loss, _bound_worker_matches, _bound_claim_is_live,
 *     worker_result, worker_exit_code, _stored_conversation, _run_task_turns,
 *     recover / status / block / turn (_run_task_turns closures).
 *   gateway/session_controls.py: kanban_run.
 *
 * Every live external call enters through ONE universal seam:
 * tmk_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Ported siblings are SEAMED inside flows (each is exercised by its
 * own direct cases, where the real body runs on both sides).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmk_-prefixed.
 *
 * Seam name -> live call (args are informational projections):
 *   build_kanban_policy      build_kanban_policy(connection, params, config)
 *                            [ported sibling; ret projects to [policy, private]]
 *   run_task                 session_kanban.run_task(connection, params)
 *                            [ported sibling; kanban_run's await]
 *   watch_submitter          _watch_submitter(Path(db), run_id)  [sibling]
 *   run_worker_turns_inner   _run_worker_turns(agent, frame, history) [sibling]
 *   worker_workspace         _worker_workspace(frame) CM [sibling; the seam
 *                            result is the yielded task_id; enter/exit silent]
 *   record_worker_result     _record_worker_result(context, code, out) [sibling]
 *   bound_worker_matches     _bound_worker_matches(conn, context) [sibling]
 *   bound_claim_is_live      _bound_claim_is_live(context) [ported, direct
 *                            cases only — the claim_check lambda is not
 *                            driven by the recover_failed_kanban_turns seam]
 *   stored_conversation      _stored_conversation(agent) [sibling]
 *   run_task_turns           _run_task_turns(agent, frame, history, ctx) [sibling]
 *   worker_result            worker_result(path, params) [sibling]
 *   kb._require_slug         kanban_db._require_slug(slug)
 *   kb.kanban_db_path        kanban_db.kanban_db_path(board=board) -> POSIX str
 *   kb.get_task              kanban_db.get_task(conn, task_id) -> Task double
 *   kb.build_worker_context  kanban_db.build_worker_context(conn, task_id)
 *   kb._retry_status_for_run kanban_db._retry_status_for_run(conn, tid, rid)
 *   kb.workspaces_root       kanban_db.workspaces_root(board=board) -> POSIX str
 *   kb.write_txn             kanban_db.write_txn(conn) CM [enter/exit silent]
 *   kb._append_event         kanban_db._append_event(conn, tid, kind, payload,
 *                            run_id=rid)
 *   kb.redact_review_value   kanban_db.redact_review_value(value)
 *   kb.goal_run_status       kanban_db.goal_run_status(conn, tid, rid)
 *   kb.block_task            kanban_db.block_task(conn, tid, reason=,
 *                            expected_run_id=)
 *   kbc.connect_closing      kanban_db_connect.connect_closing(Path(db)|none)
 *                            -> conn CM; its __exit__ fires conn.close
 *   sqlite3.connect          sqlite3.connect(uri, uri=True) -> conn double
 *   conn.execute             conn.execute(sql, params).fetchone() [the script
 *                            carries the fetchone() row; fetchone is a silent
 *                            identity projection; row shape is per call site:
 *                            dict for key access, [payload,...] for index]
 *   conn.close               conn.close() (closing()/connect_closing teardown)
 *   path.resolve             Path.resolve(strict=) -> POSIX str [monkeypatched
 *                            live; the fixture domain is POSIX paths]
 *   path.as_uri              Path.as_uri() -> scripted URI string
 *   resolve_profile_env      hermes_cli.profiles.resolve_profile_env(profile)
 *   build_policy             gateway.session_policy.build_policy(params,
 *                            config, private_secrets=private) -> policy double
 *   policy.config            policy.config() on the policy double
 *   resolve_gateway_model    gateway.run._resolve_gateway_model(config)
 *   load_gateway_config      gateway.run._load_gateway_config()
 *   restore_policy           gateway.session_policy.restore_policy(data)
 *   authority_persistence    gateway.session_persistence.authority_persistence(
 *                            authority) -> store double
 *   store_location           persistence_assembly.store_location(store.scope)
 *   store.bindings.receipt   store.bindings.receipt(sid) -> saved double|null
 *   asyncio.to_thread        asyncio.to_thread(fn, *a, **k) [fn projects to
 *                            fn.__name__; the stub does NOT call fn — the
 *                            script models the off-loop result]
 *   local_session_id         persistence.authority.bindings.local_session_id(
 *                            profile_id, subject, request_id)
 *   create_local_session     gateway.session_local.create_local_session(
 *                            authority, actor, params, trusted_policy=,
 *                            trusted_secrets=) -> ref double
 *   restore_local_session    gateway.session_local_recovery.restore_local_session(
 *                            authority, sid) -> ref double
 *   tracked_write            gateway.session_runtime_workers.tracked_write(
 *                            authority, admit_claim) [records, then runs the
 *                            closure synchronously; a scripted raise skips it]
 *   connection.resume        connection.resume(ref, {})
 *   authority.submit         authority.submit(actor, Submission(request_id,
 *                            ref, payload, "queue"), _kanban_card=card)
 *                            [submission projects to its 4 fields; receipt
 *                            script is the asdict() projection]
 *   psutil.Process           psutil.Process() -> process marker
 *   worker_birth             hermes_cli.process_identity.worker_birth(proc)
 *   os.getpid                os.getpid()
 *   os.chdir                 os.chdir(workspace)
 *   os._exit                 os._exit(1) [models the hard exit as SystemExit(1)
 *                            on both sides — the catchable analog; the seam
 *                            records, then the flow unwinds with SystemExit]
 *   kbd._worker_terminal_timeout_env  kanban_db_dispatch._worker_terminal_timeout_env(
 *                            max_runtime_seconds, os.environ.get(key))
 *   kbd._process_fingerprint kanban_db_dispatch._process_fingerprint(pid)
 *   kbd._worker_alive        kanban_db_dispatch._worker_alive(pid, started_at)
 *   threading.Thread         threading.Thread(target=watch, name=..., daemon=True)
 *   thread.start             the thread double's .start()
 *   turns_done.wait          _TURNS_DONE.wait(1.0)
 *   turns_done.set           _TURNS_DONE.set() [also sets state._TURNS_DONE]
 *   worker_turn_scope        gateway.session_managed_worker.worker_turn_scope(
 *                            frame) CM [enter/exit silent]
 *   arm_surface_turn         gateway.session_surface.arm_surface_turn(agent)
 *   worker_display_kwargs    gateway.session_display.worker_display_kwargs()
 *   agent.run_conversation   agent.run_conversation(text, conversation_history=,
 *                            **kwargs) [kwargs in live evaluation order]
 *   turn_exit_code           hermes_cli.turn_exit.turn_exit_code(result,
 *                            kanban_worker=True, **flags)
 *   credential_failure_flags hermes_cli.turn_exit.credential_failure_flags(exc)
 *   log.exception            logging.getLogger(__name__).exception(fmt)
 *                            [getLogger is a silent factory]
 *   worker_claim_is_live     agent.kanban_turn_recovery.worker_claim_is_live(
 *                            task_id=, db_path=, run_id=, claim_lock=,
 *                            verify_pid=False)
 *   agent_interface          agent.agent_init_session.agent_interface(agent)
 *   agent.get_messages_as_conversation  agent._session_db.get_messages_as_conversation(
 *                            agent.session_id)
 *   store.read_conversation  store.messages.read_conversation(agent.session_id)
 *   message.as_openai_message  each message double's .as_openai_message()
 *   build_preloaded_skills_prompt  agent.skill_commands.build_preloaded_skills_prompt(
 *                            skills, task_id=agent.session_id) -> [prompt,
 *                            loaded, missing]
 *   recover_failed_kanban_turns  agent.kanban_turn_recovery.recover_failed_kanban_turns(
 *                            recover, lambda: result, task_id=, claim_check=
 *                            lambda: _bound_claim_is_live(context)) [the stub
 *                            does NOT drive the callbacks; recover/status/
 *                            block/turn have their own direct cases; the
 *                            lambdas project to "<lambda>"]
 *   run_kanban_goal_loop     hermes_cli.goals.run_kanban_goal_loop(task_id=,
 *                            goal_text=, run_turn=turn, task_status_fn=status,
 *                            block_fn=block, max_turns=, first_response=)
 *   served_profile_name      gateway.session_authorities.served_profile_name(home)
 *   profile_from_session_key session_keys.profile_from_session_key(route)
 *   set_session_vars         gateway.session_context.set_session_vars(
 *                            session_key=, session_id=, profile=) -> tokens
 *   bind_turn_workspace      gateway.session_policy.bind_turn_workspace(policy,
 *                            session_id)
 *   clear_session_vars       gateway.session_context.clear_session_vars(tokens)
 *
 * Representation contract (shared with tests/oracle_tui_methods_kanban_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) PASS
 *   THROUGH every except-Exception site. The FULL builtin hierarchy plus
 *   AuthorityRefusal(PersistenceError, ValueError), sqlite3.Error and
 *   JSONDecodeError(ValueError) ride the tmk_exc_parents table, so
 *   except (ValueError, OSError, sqlite3.Error) catches AuthorityRefusal and
 *   JSONDecodeError exactly as live, while KeyError/TypeError pass through.
 * - AuthorityRefusal str(exc) == reason. Scripted raises of plain kinds carry
 *   str(exc) == message verbatim.
 * - Doubles: connection/actor/authority/store/ref/task/policy/agent are
 *   fixture dicts; the oracle materializes SimpleNamespaces (policy is a
 *   dynamic dataclass so replace()/asdict() run live). Seam args project
 *   markers: connection->"connection", authority->"authority", agent->"agent",
 *   conn->"conn", scope->"scope", process->"process", ref->its session_id,
 *   actor/policy/frame/params/context -> their dict projections.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints;
 *   Paths to POSIX strings (fixture paths are POSIX, no drive letters/UNC;
 *   Path.__str__/resolve/as_uri are monkeypatched live to the seam model).
 * - str()/slicing fold the ASCII fixture corpus only. json.dumps projections
 *   (request_id compact, request_json sort_keys, kanban_json insertion order)
 *   use Python default separators (', ', ': ') unless compact is pinned.
 *   Payload/kanban_json strings are VALID JSON in the fixture domain.
 * - State: ctx.state mirrors _TURNS_DONE (bool, set by _record_worker_result),
 *   os.environ (dict, updated by bind_worker_context) and the closure result
 *   box (recover/turn nonlocal). Functions that touch a key materialize it;
 *   other functions leave state alone.
 * - Closure cases (admit_claim/watch/recover/status/block/turn) drive the
 *   closure body with its captured free variables supplied as args; recover
 *   and turn write the mutated nonlocal result to state.result.
 * - _worker_workspace CM cases drive __enter__ then __exit__(None,None,None);
 *   ret is the yielded value; an enter raise unwinds through the finally.
 *
 * Ownership: tmk_call ADOPTS args into the trace event; its return is a fresh
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
typedef struct tmk_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmk_ctx_t;

/* ── basic helpers ── */
static bool tmk_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmk_raise_kind(const json_t *v) {
    return tmk_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmk_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmk_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmk_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmk_type_name(const json_t *v) {
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

static char *tmk_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for scalars (containers are outside the str()-projection
 * fixture domain; they serialize as JSON, which no fixture exercises). */
static char *tmk_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Deep scalar equality (Python == on the fixture domain; bool==int cross). */
static bool tmk_json_eq(const json_t *a, const json_t *b) {
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
            if (!tmk_json_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tmk_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* ── exception hierarchy (FULL builtin tree + the quarry customs) ──
 * Multiple inheritance is a parent LIST (AuthorityRefusal rides both
 * PersistenceError and ValueError). An unknown scripted kind defaults to
 * Exception — exactly the oracle make_exc fallback type(kind, (Exception,)).
 * BaseException-only kinds never reach Exception. */
typedef struct tmk_exc_parent {
    const char *kind;
    const char *parents[4];   /* NULL-terminated */
} tmk_exc_parent_t;

static const tmk_exc_parent_t TMK_EXC_PARENTS[] = {
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
    {"PersistenceError", {"Exception", NULL}},
    {"AuthorityRefusal", {"PersistenceError", "ValueError", NULL}},
    {"sqlite3.Error", {"Exception", NULL}},
};
#define TMK_EXC_N (sizeof(TMK_EXC_PARENTS) / sizeof(TMK_EXC_PARENTS[0]))

static const tmk_exc_parent_t *tmk_exc_entry(const char *kind) {
    for (size_t i = 0; i < TMK_EXC_N; i++)
        if (!strcmp(TMK_EXC_PARENTS[i].kind, kind)) return &TMK_EXC_PARENTS[i];
    return NULL;
}

/* isinstance(kind, ancestor) over the parent tree; unknown kinds are
 * Exception subclasses (the make_exc fallback). */
static bool tmk_exc_isa(const char *kind, const char *ancestor) {
    if (!kind) return false;
    if (!strcmp(kind, ancestor)) return true;
    const tmk_exc_parent_t *e = tmk_exc_entry(kind);
    if (!e) return !strcmp(ancestor, "Exception") || !strcmp(ancestor, "BaseException");
    for (size_t i = 0; i < 4 && e->parents[i]; i++)
        if (tmk_exc_isa(e->parents[i], ancestor)) return true;
    return false;
}

/* BaseException-only kinds pass through every except-Exception site. */
static bool tmk_is_base_exc(const char *kind) {
    return kind && !tmk_exc_isa(kind, "Exception");
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmk_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmk_err_spec(type, msg);
}

static json_t *tmk_call(tmk_ctx_t *ctx, const char *name, json_t *args) {
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
        ret = tmk_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmk_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmk_make_exc(raise);
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
static json_t *tmk_args0(void) { return json_object(); }
static void tmk_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmk_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmk_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmk_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }/* ── growable string buffer ── */
typedef struct tmk_sbuf { char *buf; size_t len, cap; } tmk_sbuf_t;

static void tmk_sb_init(tmk_sbuf_t *s) {
    s->cap = 128; s->len = 0;
    s->buf = malloc(s->cap);
    if (s->buf) s->buf[0] = '\0';
}
static void tmk_sb_grow(tmk_sbuf_t *s, size_t need) {
    if (!s->buf || s->len + need + 1 <= s->cap) return;
    while (s->cap < s->len + need + 1) s->cap *= 2;
    char *nb = realloc(s->buf, s->cap);
    if (nb) s->buf = nb;
}
static void tmk_sb_putc(tmk_sbuf_t *s, char c) {
    tmk_sb_grow(s, 1);
    if (!s->buf) return;
    s->buf[s->len++] = c;
    s->buf[s->len] = '\0';
}
static void tmk_sb_puts(tmk_sbuf_t *s, const char *p) {
    if (!p) p = "";
    size_t n = strlen(p);
    tmk_sb_grow(s, n);
    if (!s->buf) return;
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

/* Python json.dumps string body (ensure_ascii; ASCII fixture domain). */
static void tmk_dump_string(tmk_sbuf_t *s, const char *v) {
    tmk_sb_putc(s, '"');
    for (const unsigned char *p = (const unsigned char *)(v ? v : ""); *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': tmk_sb_puts(s, "\\\""); break;
            case '\\': tmk_sb_puts(s, "\\\\"); break;
            case '\n': tmk_sb_puts(s, "\\n"); break;
            case '\r': tmk_sb_puts(s, "\\r"); break;
            case '\t': tmk_sb_puts(s, "\\t"); break;
            case '\b': tmk_sb_puts(s, "\\b"); break;
            case '\f': tmk_sb_puts(s, "\\f"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
                    tmk_sb_puts(s, esc);
                } else {
                    tmk_sb_putc(s, (char)c);
                }
        }
    }
    tmk_sb_putc(s, '"');
}

static int tmk_key_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* json.dumps(v, sort_keys=?) — Python default separators (', ', ': ') unless
 * compact pins (',', ':'). Fixture domain: str/int/bool/null/list/dict. */
static void tmk_dumps_rec(tmk_sbuf_t *s, const json_t *v, bool sort_keys, bool compact) {
    const char *item_sep = compact ? "," : ", ";
    const char *kv_sep = compact ? ":" : ": ";
    if (!v || v->type == JSON_NULL) { tmk_sb_puts(s, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: tmk_sb_puts(s, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            tmk_sb_puts(s, buf);
            return;
        }
        case JSON_STRING: tmk_dump_string(s, v->str_val); return;
        case JSON_ARRAY:
            tmk_sb_putc(s, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tmk_sb_puts(s, item_sep);
                tmk_dumps_rec(s, v->c.items[i], sort_keys, compact);
            }
            tmk_sb_putc(s, ']');
            return;
        case JSON_OBJECT: {
            tmk_sb_putc(s, '{');
            size_t n = v->c.count;
            const char **keys = malloc(n * sizeof(char *));
            if (!keys) { tmk_sb_putc(s, '}'); return; }
            for (size_t i = 0; i < n; i++) keys[i] = v->c.keys[i];
            if (sort_keys && n > 1) qsort(keys, n, sizeof(char *), tmk_key_cmp);
            for (size_t i = 0; i < n; i++) {
                if (i) tmk_sb_puts(s, item_sep);
                tmk_dump_string(s, keys[i]);
                tmk_sb_puts(s, kv_sep);
                tmk_dumps_rec(s, json_obj_get(v, keys[i]), sort_keys, compact);
            }
            free(keys);
            tmk_sb_putc(s, '}');
            return;
        }
        default: tmk_sb_puts(s, "null"); return;
    }
}

static char *tmk_dumps(const json_t *v, bool sort_keys, bool compact) {
    tmk_sbuf_t s;
    tmk_sb_init(&s);
    tmk_dumps_rec(&s, v, sort_keys, compact);
    return s.buf ? s.buf : tmk_strdup("null");
}

/* json.loads on the VALID-JSON fixture domain. A parse failure projects to
 * JSONDecodeError (the common live message); malformed payloads are outside
 * the fixture domain and no case exercises them. */
static json_t *tmk_loads(const char *s, json_t **raise_out) {
    *raise_out = NULL;
    json_t *v = json_parse(s ? s : "", NULL);
    if (!v) {
        *raise_out = tmk_err_spec("JSONDecodeError",
                                  "Expecting value: line 1 column 1 (char 0)");
        return NULL;
    }
    return v;
}

/* int() coercion (fixture domain: integral numbers, bools, digit strings).
 * Truncates toward zero; the int-strip whitespace set is White_Space minus
 * \x1c-\x1f (str.strip's set is wider). */
static bool tmk_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f
        || c == 0x85 || c == 0xa0;
}
/* int() does NOT strip \x1c-\x1f (unlike str.strip). */
static bool tmk_is_int_strip_ws(unsigned char c) {
    return tmk_is_int_ws(c) && !(c >= 0x1c && c <= 0x1f);
}

static json_t *tmk_pyint(const json_t *v, json_t **raise_out) {
    *raise_out = NULL;
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        double t = (d < 0) ? -d : d;
        long long whole = (long long)t;
        return json_number((d < 0) ? -(double)whole : (double)whole);
    }
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && tmk_is_int_strip_ws((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && tmk_is_int_strip_ws((unsigned char)end[-1])) end--;
        const char *p = s;
        bool neg = false;
        if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        if (p == end) goto bad;
        long long acc = 0;
        for (; p < end; p++) {
            if (*p < '0' || *p > '9') goto bad;
            acc = acc * 10 + (*p - '0');
        }
        return json_number(neg ? -(double)acc : (double)acc);
    bad: {
            char *rep = tmk_pystr(v);
            char msg[256];
            snprintf(msg, sizeof msg,
                     "invalid literal for int() with base 10: '%s'", rep ? rep : "");
            free(rep);
            *raise_out = tmk_err_spec("ValueError", msg);
            return NULL;
        }
    }
    char msg[160];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             tmk_type_name(v));
    *raise_out = tmk_err_spec("TypeError", msg);
    return NULL;
}

/* byte slices over the ASCII corpus: s[-n:] and s[:n] */
static char *tmk_slice_tail(const char *s, size_t n) {
    if (!s) s = "";
    size_t len = strlen(s);
    const char *start = (len > n) ? s + (len - n) : s;
    return tmk_strdup(start);
}
static char *tmk_slice_head(const char *s, size_t n) {
    if (!s) s = "";
    size_t len = strlen(s);
    if (len > n) {
        char *o = malloc(n + 1);
        if (o) { memcpy(o, s, n); o[n] = '\0'; }
        return o;
    }
    return tmk_strdup(s);
}

/* list(dict.fromkeys(xs)) — order-preserving string dedup. */
static json_t *tmk_dedup(const json_t *arr) {
    json_t *o = json_array();
    if (!arr || arr->type != JSON_ARRAY) return o;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        bool seen = false;
        for (size_t j = 0; j < o->c.count; j++)
            if (tmk_json_eq(o->c.items[j], v)) { seen = true; break; }
        if (!seen) json_append(o, json_copy(v));
    }
    return o;
}

/* sorted(set(xs) | {extra}) — string domain. */
static json_t *tmk_sorted_union(const json_t *arr, const char *extra) {
    json_t *all = json_array();
    if (arr && arr->type == JSON_ARRAY)
        for (size_t i = 0; i < arr->c.count; i++)
            json_append(all, json_copy(arr->c.items[i]));
    if (extra) json_append(all, json_string(extra));
    json_t *dedup = tmk_dedup(all);
    json_free(all);
    size_t n = dedup->c.count;
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            const char *a = tmk_str(dedup->c.items[i]);
            const char *b = tmk_str(dedup->c.items[j]);
            if (a && b && strcmp(a, b) > 0) {
                json_t *t = dedup->c.items[i];
                dedup->c.items[i] = dedup->c.items[j];
                dedup->c.items[j] = t;
            }
        }
    return dedup;
}

/* KeyError spec: str(KeyError(k)) == repr(k) (ASCII single-quoted). */
static json_t *tmk_key_error(const char *key) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s'", key ? key : "");
    return tmk_err_spec("KeyError", msg);
}

/* dict subscript: absent key raises KeyError; present null is a value. */
static const json_t *tmk_sub(const json_t *obj, const char *key, json_t **raise_out) {
    *raise_out = NULL;
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) {
        if (!obj || obj->type != JSON_OBJECT) {
            char msg[160];
            snprintf(msg, sizeof msg, "'%s' object is not subscriptable",
                     tmk_type_name(obj));
            *raise_out = tmk_err_spec("TypeError", msg);
        } else {
            *raise_out = tmk_key_error(key);
        }
    }
    return v;
}

/* attribute read on a double: absent attribute raises AttributeError with
 * the double's live type name (SimpleNamespace / dynamic Policy dataclass). */
static const json_t *tmk_attr(const json_t *obj, const char *key,
                              const char *typename_, json_t **raise_out) {
    *raise_out = NULL;
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                 typename_ ? typename_ : "types.SimpleNamespace", key);
        *raise_out = tmk_err_spec("AttributeError", msg);
    }
    return v;
}

#define TMK_NS "types.SimpleNamespace"

/* set(params) key-set equality against a fixed key list. */
static bool tmk_keyset_is(const json_t *obj, const char *const *keys, size_t n) {
    if (!obj || obj->type != JSON_OBJECT || obj->c.count != n) return false;
    for (size_t i = 0; i < n; i++)
        if (!json_obj_get(obj, keys[i])) return false;
    return true;
}

/* 'v' in list (string membership). */
static bool tmk_in_list(const json_t *arr, const char *v) {
    if (!arr || arr->type != JSON_ARRAY || !v) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *s = tmk_str(arr->c.items[i]);
        if (s && !strcmp(s, v)) return true;
    }
    return false;
}

/* ── shared seam wrappers ── */

/* os.getpid() */
static json_t *tmk_getpid(tmk_ctx_t *ctx) {
    return tmk_call(ctx, "os.getpid", tmk_args0());
}

/* conn.close() — teardown; result checked by the caller's CM model. */
static json_t *tmk_conn_close(tmk_ctx_t *ctx) {
    return tmk_call(ctx, "conn.close", tmk_args0());
}

/* connect_closing(Path(db)) / connect_closing() — conn CM; its exit fires
 * conn.close. The seam ret is unused (conn identity is implicit). */
static json_t *tmk_connect_closing(tmk_ctx_t *ctx, const char *db) {
    json_t *a = tmk_args0();
    tmk_asets(a, "path", db);
    return tmk_call(ctx, "kbc.connect_closing", a);
}

/* kb.write_txn(conn) CM — enter/exit silent. */
static json_t *tmk_write_txn(tmk_ctx_t *ctx) {
    json_t *a = tmk_args0();
    tmk_asets(a, "conn", "conn");
    return tmk_call(ctx, "kb.write_txn", a);
}

/* sqlite3.connect(path.as_uri() + '?mode=ro', uri=True) — the resolve/as_uri
 * projections fire first, exactly as live evaluates them. */
static json_t *tmk_sqlite_connect_ro(tmk_ctx_t *ctx, const char *path,
                                     bool resolve_first) {
    json_t *r = NULL;
    const char *resolved = path;
    if (resolve_first) {
        json_t *a = tmk_args0();
        tmk_asets(a, "path", path);
        tmk_asetb(a, "strict", false);
        r = tmk_call(ctx, "path.resolve", a);
        if (tmk_is_raise(r)) return r;
        resolved = tmk_str(r);
        if (!resolved) resolved = "";
    }
    json_t *a2 = tmk_args0();
    tmk_asets(a2, "path", resolved);
    json_t *uri = tmk_call(ctx, "path.as_uri", a2);
    if (tmk_is_raise(uri)) { json_free(r); return uri; }
    const char *us = tmk_str(uri);
    char *full = malloc((us ? strlen(us) : 0) + 16);
    if (full) sprintf(full, "%s?mode=ro", us ? us : "");
    json_t *a3 = tmk_args0();
    tmk_asets(a3, "path", full ? full : "?mode=ro");
    tmk_asetb(a3, "uri", true);
    json_t *conn = tmk_call(ctx, "sqlite3.connect", a3);
    free(full);
    json_free(uri);
    json_free(r);
    return conn;
}

/* conn.execute(sql, params).fetchone() — the script carries the row. */
static json_t *tmk_execute(tmk_ctx_t *ctx, const char *sql, json_t *params) {
    json_t *a = tmk_args0();
    tmk_asets(a, "sql", sql);
    json_set(a, "params", params ? params : json_array());
    return tmk_call(ctx, "conn.execute", a);
}

/* kb._append_event(conn, task_id, kind, payload, run_id=run_id) — payload
 * ADOPTED. */
static json_t *tmk_append_event(tmk_ctx_t *ctx, const char *task_id,
                                const char *kind, json_t *payload,
                                const json_t *run_id) {
    json_t *a = tmk_args0();
    tmk_asets(a, "conn", "conn");
    tmk_asets(a, "task_id", task_id);
    tmk_asets(a, "kind", kind);
    json_set(a, "payload", payload ? payload : json_null());
    tmk_aset(a, "run_id", run_id);
    return tmk_call(ctx, "kb._append_event", a);
}

/* Path(db).resolve() projection (strict=False). */
static json_t *tmk_path_resolve(tmk_ctx_t *ctx, const char *path, bool strict) {
    json_t *a = tmk_args0();
    tmk_asets(a, "path", path);
    tmk_asetb(a, "strict", strict);
    return tmk_call(ctx, "path.resolve", a);
}/* ── leaf flows ── */

/* PoP: tmk_worker_result @ gateway/session_kanban.py:worker_result */
static json_t *tmk_worker_result(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *path = json_obj_get(args, "path");
    const json_t *params = json_obj_get(args, "params");
    json_t *r = tmk_sqlite_connect_ro(ctx, tmk_str(path), true);
    if (tmk_is_raise(r)) return r;
    json_free(r);
    json_t *ret = NULL;
    json_t *err = NULL;
    const json_t *run_id = tmk_sub(params, "run_id", &err);
    if (err) { ret = err; goto close; }
    const json_t *task_id = tmk_sub(params, "task_id", &err);
    if (err) { ret = err; goto close; }
    json_t *p = json_array();
    json_append(p, json_copy(run_id));
    json_append(p, json_copy(task_id));
    json_t *row = tmk_execute(ctx,
        "SELECT payload FROM task_events WHERE run_id=? AND task_id=? "
        "AND kind='worker_result' ORDER BY id DESC LIMIT 1", p);
    if (tmk_is_raise(row)) { ret = row; goto close; }
    json_t *result = NULL;
    if (tmk_truthy(row)) {
        const json_t *payload = json_get(row, 0);
        result = tmk_loads(tmk_str(payload), &err);
        if (err) { json_free(row); ret = err; goto close; }
    } else {
        result = json_object();
    }
    json_free(row);
    const json_t *claim = tmk_sub(params, "claim_lock", &err);
    if (err) { json_free(result); ret = err; goto close; }
    const json_t *rc = json_obj_get(result, "claim_lock");
    if (tmk_json_eq(rc, claim)) {
        ret = result;
    } else {
        json_free(result);
        ret = json_object();
    }
close:
    ;
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);            /* body raise propagates; close is context */
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    return ret ? ret : json_null();
}

/* PoP: tmk_worker_exit_code @ gateway/session_kanban.py:worker_exit_code */
static json_t *tmk_worker_exit_code(tmk_ctx_t *ctx, const json_t *args) {
    json_t *a = tmk_args0();
    tmk_aset(a, "path", json_obj_get(args, "path"));
    tmk_aset(a, "params", json_obj_get(args, "params"));
    json_t *result = tmk_call(ctx, "worker_result", a);
    if (tmk_is_raise(result)) return result;
    const json_t *code = json_obj_get(result, "exit_code");
    json_t *ret = code ? json_copy(code) : json_number(1);
    json_free(result);
    return ret;
}

/* PoP: tmk_bound_worker_matches @ gateway/session_kanban.py:_bound_worker_matches */
static json_t *tmk_bound_worker_matches(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    json_t *err = NULL;
    const json_t *task_id = tmk_sub(context, "task_id", &err);
    if (err) return err;
    const json_t *run_id = tmk_sub(context, "run_id", &err);
    if (err) return err;
    json_t *p = json_array();
    json_append(p, json_copy(task_id));
    json_append(p, json_copy(run_id));
    json_t *row = tmk_execute(ctx,
        "SELECT payload FROM task_events WHERE task_id=? AND run_id=? "
        "AND kind='worker_bound' ORDER BY id DESC LIMIT 1", p);
    if (tmk_is_raise(row)) return row;
    json_t *bound = NULL;
    if (tmk_truthy(row)) {
        const json_t *payload = json_get(row, 0);
        bound = tmk_loads(tmk_str(payload), &err);
        if (err) { json_free(row); return err; }
    } else {
        bound = json_object();
    }
    json_free(row);
    json_t *pid = tmk_getpid(ctx);
    if (tmk_is_raise(pid)) { json_free(bound); return pid; }
    const json_t *claim = tmk_sub(context, "claim_lock", &err);
    if (err) { json_free(bound); json_free(pid); return err; }
    const json_t *bp = json_obj_get(bound, "pid");
    const json_t *bc = json_obj_get(bound, "claim_lock");
    bool match = tmk_json_eq(bp, pid) && tmk_json_eq(bc, claim);
    json_free(bound);
    json_free(pid);
    return json_bool(match);
}

/* PoP: tmk_bound_claim_is_live @ gateway/session_kanban.py:_bound_claim_is_live */
static json_t *tmk_bound_claim_is_live(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    json_t *err = NULL;
    const json_t *task_id = tmk_sub(context, "task_id", &err);
    if (err) return err;
    const json_t *db = tmk_sub(context, "db", &err);
    if (err) return err;
    const json_t *run_id = tmk_sub(context, "run_id", &err);
    if (err) return err;
    char *rid_str = tmk_pystr(run_id);
    const json_t *claim = tmk_sub(context, "claim_lock", &err);
    if (err) { free(rid_str); return err; }
    json_t *a = tmk_args0();
    tmk_asets(a, "task_id", tmk_str(task_id));
    tmk_asets(a, "db_path", tmk_str(db));
    tmk_asets(a, "run_id", rid_str);
    tmk_asets(a, "claim_lock", tmk_str(claim));
    tmk_asetb(a, "verify_pid", false);
    free(rid_str);
    json_t *live = tmk_call(ctx, "worker_claim_is_live", a);
    if (tmk_is_raise(live)) return live;
    bool is_live = tmk_truthy(live);
    json_free(live);
    if (!is_live) return json_bool(false);
    /* try: with closing(sqlite3.connect(...)) as conn: return matches(...)
     * except sqlite3.Error: return False */
    json_t *r = tmk_sqlite_connect_ro(ctx, tmk_str(db), true);
    if (tmk_is_raise(r)) {
        if (tmk_exc_isa(tmk_raise_kind(r), "sqlite3.Error")) {
            json_free(r);
            return json_bool(false);
        }
        return r;
    }
    json_free(r);
    json_t *a2 = tmk_args0();
    tmk_asets(a2, "conn", "conn");
    tmk_aset(a2, "context", context);
    json_t *ret = tmk_call(ctx, "bound_worker_matches", a2);
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
        if (tmk_exc_isa(tmk_raise_kind(ret), "sqlite3.Error")) {
            json_free(ret);
            return json_bool(false);
        }
        return ret;
    }
    if (tmk_is_raise(cr)) {
        json_free(ret);
        if (tmk_exc_isa(tmk_raise_kind(cr), "sqlite3.Error")) {
            json_free(cr);
            return json_bool(false);
        }
        return cr;
    }
    json_free(cr);
    return ret;
}

/* PoP: tmk_stored_conversation @ gateway/session_kanban.py:_stored_conversation */
static json_t *tmk_stored_conversation(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *agent = json_obj_get(args, "agent");
    json_t *a = tmk_args0();
    tmk_asets(a, "agent", "agent");
    json_t *store = tmk_call(ctx, "agent_interface", a);
    if (tmk_is_raise(store)) return store;
    json_t *err = NULL;
    if (!store || store->type == JSON_NULL) {
        json_free(store);
        const json_t *sdb = tmk_attr(agent, "_session_db", TMK_NS, &err);
        if (err) return err;
        (void)sdb;
        const json_t *sid = tmk_attr(agent, "session_id", TMK_NS, &err);
        if (err) return err;
        json_t *a2 = tmk_args0();
        tmk_aset(a2, "session_id", sid);
        return tmk_call(ctx, "agent.get_messages_as_conversation", a2);
    }
    const json_t *messages = tmk_attr(store, "messages", TMK_NS, &err);
    if (err) { json_free(store); return err; }
    (void)messages;
    const json_t *sid = tmk_attr(agent, "session_id", TMK_NS, &err);
    if (err) { json_free(store); return err; }
    json_t *a2 = tmk_args0();
    tmk_aset(a2, "session_id", sid);
    json_t *rows = tmk_call(ctx, "store.read_conversation", a2);
    json_free(store);
    if (tmk_is_raise(rows)) return rows;
    json_t *out = json_array();
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < rows->c.count; i++) {
            json_t *m = tmk_call(ctx, "message.as_openai_message", tmk_args0());
            if (tmk_is_raise(m)) {
                json_free(out);
                json_free(rows);
                return m;
            }
            json_append(out, m);
        }
    }
    json_free(rows);
    return out;
}

/* PoP: tmk_record_worker_loss @ gateway/session_kanban.py:record_worker_loss */
static json_t *tmk_record_worker_loss(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *policy = json_obj_get(args, "policy");
    const json_t *reason = json_obj_get(args, "reason");
    const json_t *pid = json_obj_get(args, "pid");
    const json_t *bound = json_obj_get(args, "bound");
    json_t *ret = NULL;
    json_t *err = NULL;
    const json_t *kj = tmk_attr(policy, "kanban_json", TMK_NS, &err);
    if (err) { ret = err; goto handled; }
    const char *kjs = tmk_str(kj);
    json_t *context = tmk_loads(tmk_truthy(kj) ? kjs : "null", &err);
    if (err) { ret = err; goto handled; }
    if (!tmk_truthy(context)) { json_free(context); ret = json_null(); goto done; }
    const json_t *rid_v = tmk_sub(context, "run_id", &err);
    if (err) { json_free(context); ret = err; goto handled; }
    json_t *run_id = tmk_pyint(rid_v, &err);
    if (err) { json_free(context); ret = err; goto handled; }
    const json_t *db = tmk_sub(context, "db", &err);
    if (err) { json_free(context); json_free(run_id); ret = err; goto handled; }
    json_t *r = tmk_connect_closing(ctx, tmk_str(db));
    if (tmk_is_raise(r)) {
        json_free(context); json_free(run_id); ret = r; goto handled;
    }
    json_free(r);
    r = tmk_write_txn(ctx);
    if (tmk_is_raise(r)) {
        json_free(context); json_free(run_id);
        json_t *cr = tmk_conn_close(ctx);
        json_free(cr);
        ret = r; goto handled;
    }
    json_free(r);
    const json_t *tid = tmk_sub(context, "task_id", &err);
    if (err) { ret = err; goto body_close; }
    json_t *p = json_array();
    json_append(p, json_copy(tid));
    json_t *row = tmk_execute(ctx,
        "SELECT status, current_run_id FROM tasks WHERE id=?", p);
    if (tmk_is_raise(row)) { ret = row; goto body_close; }
    bool drop = false;
    if (!row || row->type == JSON_NULL) {
        drop = true;
    } else {
        const json_t *status = json_get(row, 0);
        const json_t *cur = json_get(row, 1);
        const char *ss = tmk_str(status);
        if (!ss || strcmp(ss, "running") != 0 || !tmk_json_eq(cur, run_id))
            drop = true;
    }
    json_free(row);
    if (drop) { ret = json_null(); goto body_close; }
    char *rs = tmk_pystr(reason);
    char *reason_cut = tmk_slice_head(rs, 200);
    free(rs);
    const json_t *claim = tmk_sub(context, "claim_lock", &err);
    if (err) { free(reason_cut); ret = err; goto body_close; }
    json_t *payload = json_object();
    tmk_asets(payload, "reason", reason_cut);
    free(reason_cut);
    tmk_asets(payload, "claim_lock", tmk_str(claim));
    tmk_aset(payload, "run_id", run_id);
    tmk_aset(payload, "pid", pid);
    tmk_asetb(payload, "bound", tmk_truthy(bound));
    ret = tmk_append_event(ctx, tmk_str(tid), "worker_lost", payload, run_id);
body_close:
    ;
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    json_free(context);
    json_free(run_id);
    goto handled;
done:
    goto out;
handled:
    if (tmk_is_raise(ret)) {
        if (tmk_is_base_exc(tmk_raise_kind(ret))) goto out;
        /* except Exception: logging.getLogger(__name__).exception(fmt) */
        json_t *a = tmk_args0();
        tmk_asets(a, "fmt", "kanban worker_lost record failed");
        json_t *lr = tmk_call(ctx, "log.exception", a);
        if (tmk_is_raise(lr)) {
            json_free(ret);
            return lr;      /* a raise inside the handler propagates */
        }
        json_free(lr);
        json_free(ret);
        ret = json_null();
    }
out:
    return ret ? ret : json_null();
}

/* PoP: tmk_record_worker_result @ gateway/session_kanban.py:_record_worker_result */
static json_t *tmk_record_worker_result(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    const json_t *code = json_obj_get(args, "code");
    const json_t *last_output = json_obj_get(args, "last_output");
    /* _TURNS_DONE.set() — the state key is materialized even when set raises */
    if (!json_obj_get(ctx->state, "_TURNS_DONE"))
        json_set(ctx->state, "_TURNS_DONE", json_bool(false));
    json_t *sr = tmk_call(ctx, "turns_done.set", tmk_args0());
    if (tmk_is_raise(sr)) return sr;
    json_set(ctx->state, "_TURNS_DONE", json_bool(true));
    json_free(sr);
    json_t *err = NULL;
    const json_t *db = tmk_sub(context, "db", &err);
    if (err) return err;
    json_t *r = tmk_connect_closing(ctx, tmk_str(db));
    if (tmk_is_raise(r)) return r;
    json_free(r);
    r = tmk_write_txn(ctx);
    if (tmk_is_raise(r)) {
        json_t *cr = tmk_conn_close(ctx);
        json_free(cr);
        return r;
    }
    json_free(r);
    json_t *a = tmk_args0();
    tmk_asets(a, "conn", "conn");
    tmk_aset(a, "context", context);
    json_t *matches = tmk_call(ctx, "bound_worker_matches", a);
    json_t *ret = NULL;
    if (tmk_is_raise(matches)) {
        ret = matches;
    } else if (tmk_truthy(matches)) {
        json_free(matches);
        /* arg order: conn, context['task_id'], payload{pid: os.getpid(),
         * claim_lock: context['claim_lock'], ...}, run_id=context['run_id'] */
        const json_t *tid = tmk_sub(context, "task_id", &err);
        if (err) {
            ret = err;
        } else {
            json_t *pid = tmk_getpid(ctx);
            if (tmk_is_raise(pid)) {
                ret = pid;
            } else {
                const json_t *claim = tmk_sub(context, "claim_lock", &err);
                if (err) {
                    json_free(pid);
                    ret = err;
                } else {
                    const json_t *run_id = tmk_sub(context, "run_id", &err);
                    if (err) {
                        json_free(pid);
                        ret = err;
                    } else {
                        json_t *payload = json_object();
                        tmk_aset(payload, "pid", pid);
                        tmk_asets(payload, "claim_lock", tmk_str(claim));
                        tmk_aset(payload, "exit_code", code);
                        tmk_aset(payload, "last_output", last_output);
                        ret = tmk_append_event(ctx, tmk_str(tid), "worker_result",
                                               payload, run_id);
                    }
                }
                json_free(pid);
            }
        }
    } else {
        json_free(matches);
        ret = json_null();
    }
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    return ret ? ret : json_null();
}

/* PoP: tmk_record_start_failure @ gateway/session_kanban.py:record_start_failure */
static json_t *tmk_record_start_failure(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *frame = json_obj_get(args, "frame");
    const json_t *exc = json_obj_get(args, "exc");
    json_t *err = NULL;
    const json_t *policy = tmk_sub(frame, "policy", &err);
    if (err) return err;
    const json_t *kj = json_obj_get(policy, "kanban_json");
    json_t *context = tmk_loads(tmk_truthy(kj) ? tmk_str(kj) : "null", &err);
    if (err) return err;
    if (!context || context->type == JSON_NULL) {
        json_free(context);
        return json_null();
    }
    /* credential_failure_flags(exc) evaluated first, then turn_exit_code */
    json_t *a = tmk_args0();
    if (exc && exc->type == JSON_OBJECT) {
        json_t *proj = json_object();
        tmk_asets(proj, "__raise__", json_get_str(exc, "type", "RuntimeError"));
        tmk_asets(proj, "message", json_get_str(exc, "message", ""));
        json_set(a, "exc", proj);
    } else {
        tmk_aset(a, "exc", exc);
    }
    json_t *flags = tmk_call(ctx, "credential_failure_flags", a);
    if (tmk_is_raise(flags)) { json_free(context); return flags; }
    json_t *a2 = tmk_args0();
    tmk_aset(a2, "result", NULL);   /* None — dropped by the seam */
    tmk_asetb(a2, "kanban_worker", true);
    if (flags && flags->type == JSON_OBJECT) {
        for (size_t i = 0; i < flags->c.count; i++)
            json_set(a2, flags->c.keys[i], json_copy(flags->c.items[i]));
    }
    json_free(flags);
    json_t *code = tmk_call(ctx, "turn_exit_code", a2);
    if (tmk_is_raise(code)) { json_free(context); return code; }
    json_t *a3 = tmk_args0();
    tmk_aset(a3, "context", context);
    tmk_aset(a3, "code", code);
    tmk_asets(a3, "last_output", "");
    json_t *ret = tmk_call(ctx, "record_worker_result", a3);
    json_free(context);
    json_free(code);
    return ret;
}/* ── worker-binding flows ── */

/* PoP: tmk_bind_worker_context @ gateway/session_kanban.py:bind_worker_context */
static json_t *tmk_bind_worker_context(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *frame = json_obj_get(args, "frame");
    json_t *err = NULL;
    /* materialize the os.environ mirror */
    if (!json_obj_get(ctx->state, "environ"))
        json_set(ctx->state, "environ", json_object());
    json_t *env_mirror = json_obj_get(ctx->state, "environ");
    const json_t *policy = tmk_sub(frame, "policy", &err);
    if (err) return err;
    const json_t *kj = json_obj_get(policy, "kanban_json");
    json_t *context = tmk_loads(tmk_truthy(kj) ? tmk_str(kj) : "null", &err);
    if (err) return err;
    if (!context || context->type == JSON_NULL) {
        json_free(context);
        return json_null();
    }
    const json_t *source = tmk_sub(policy, "source", &err);
    if (err) { json_free(context); return err; }
    const char *ss = tmk_str(source);
    if (!ss || strcmp(ss, "kanban") != 0) {
        json_free(context);
        return tmk_err_spec("ValueError", "invalid_kanban_policy");
    }
    json_t *env = json_object();
    json_t *ret = NULL;
    const json_t *tid = tmk_sub(context, "task_id", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_TASK", tmk_str(tid));
    const json_t *board = tmk_sub(context, "board", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_BOARD", tmk_str(board));
    const json_t *db = tmk_sub(context, "db", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_DB", tmk_str(db));
    const json_t *rid = tmk_sub(context, "run_id", &err);
    if (err) { ret = err; goto fail; }
    char *rid_str = tmk_pystr(rid);
    tmk_asets(env, "HERMES_KANBAN_RUN_ID", rid_str);
    free(rid_str);
    const json_t *claim = tmk_sub(context, "claim_lock", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_CLAIM_LOCK", tmk_str(claim));
    const json_t *ws = tmk_sub(context, "workspace", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_WORKSPACE", tmk_str(ws));
    const json_t *wsr = tmk_sub(context, "workspaces_root", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_KANBAN_WORKSPACES_ROOT", tmk_str(wsr));
    const json_t *profile = tmk_sub(context, "profile", &err);
    if (err) { ret = err; goto fail; }
    tmk_asets(env, "HERMES_PROFILE", tmk_str(profile));
    tmk_asets(env, "HERMES_SESSION_SOURCE", "kanban");
    const json_t *branch = tmk_sub(context, "branch", &err);
    if (err) { ret = err; goto fail; }
    if (tmk_truthy(branch)) tmk_asets(env, "HERMES_KANBAN_BRANCH", tmk_str(branch));
    const json_t *tenant = tmk_sub(context, "tenant", &err);
    if (err) { ret = err; goto fail; }
    if (tmk_truthy(tenant)) tmk_asets(env, "HERMES_TENANT", tmk_str(tenant));
    const json_t *goal_mode = tmk_sub(context, "goal_mode", &err);
    if (err) { ret = err; goto fail; }
    if (tmk_truthy(goal_mode)) tmk_asets(env, "HERMES_KANBAN_GOAL_MODE", "1");
    for (int i = 0; i < 2; i++) {
        const char *key = i == 0 ? "TERMINAL_TIMEOUT" : "TERMINAL_MAX_FOREGROUND_TIMEOUT";
        const json_t *mrs = tmk_sub(context, "max_runtime_seconds", &err);
        if (err) { ret = err; goto fail; }
        const json_t *current = json_obj_get(env_mirror, key);
        json_t *a = tmk_args0();
        tmk_aset(a, "max_runtime_seconds", mrs);
        tmk_aset(a, "current", current);
        json_t *value = tmk_call(ctx, "kbd._worker_terminal_timeout_env", a);
        if (tmk_is_raise(value)) { ret = value; goto fail; }
        if (tmk_truthy(value)) tmk_asets(env, key, tmk_str(value));
        json_free(value);
    }
    /* with connect_closing(Path(context['db'])) as conn: with write_txn: */
    const json_t *db2 = tmk_sub(context, "db", &err);
    if (err) { ret = err; goto fail; }
    json_t *r = tmk_connect_closing(ctx, tmk_str(db2));
    if (tmk_is_raise(r)) { ret = r; goto fail; }
    json_free(r);
    r = tmk_write_txn(ctx);
    if (tmk_is_raise(r)) {
        json_t *cr = tmk_conn_close(ctx);
        json_free(cr);
        ret = r; goto fail;
    }
    json_free(r);
    const json_t *tid2 = tmk_sub(context, "task_id", &err);
    if (err) { ret = err; goto body_close; }
    json_t *a = tmk_args0();
    tmk_asets(a, "conn", "conn");
    tmk_asets(a, "task_id", tmk_str(tid2));
    json_t *task = tmk_call(ctx, "kb.get_task", a);
    if (tmk_is_raise(task)) { ret = task; goto body_close; }
    bool stale = false;
    if (!task || task->type == JSON_NULL) {
        stale = true;
    } else {
        const json_t *status = tmk_attr(task, "status", TMK_NS, &err);
        if (err) { json_free(task); ret = err; goto body_close; }
        const char *st = tmk_str(status);
        if (!st || strcmp(st, "running") != 0) {
            stale = true;
        } else {
            const json_t *crid = tmk_attr(task, "current_run_id", TMK_NS, &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            const json_t *ctxrid = tmk_sub(context, "run_id", &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            if (!tmk_json_eq(crid, ctxrid)) {
                stale = true;
            } else {
                const json_t *tclaim = tmk_attr(task, "claim_lock", TMK_NS, &err);
                if (err) { json_free(task); ret = err; goto body_close; }
                const json_t *cclaim = tmk_sub(context, "claim_lock", &err);
                if (err) { json_free(task); ret = err; goto body_close; }
                if (!tmk_json_eq(tclaim, cclaim)) stale = true;
            }
        }
    }
    if (stale) {
        json_free(task);
        ret = tmk_err_spec("AuthorityRefusal", "stale_kanban_claim");
        goto body_close;
    }
    /* fingerprint = _process_fingerprint(os.getpid()) or UNVERIFIED... */
    json_t *pid0 = tmk_getpid(ctx);
    if (tmk_is_raise(pid0)) { json_free(task); ret = pid0; goto body_close; }
    json_t *fa = tmk_args0();
    tmk_aset(fa, "pid", pid0);
    json_t *fp = tmk_call(ctx, "kbd._process_fingerprint", fa);
    json_free(pid0);
    if (tmk_is_raise(fp)) { json_free(task); ret = fp; goto body_close; }
    const char *fingerprint = tmk_truthy(fp) ? tmk_str(fp) : "unverified";
    /* _append_event(conn, task.id, 'worker_bound', {pid: os.getpid(),
     * claim_lock: context['claim_lock'], started_at: fp}, run_id=...) */
    const json_t *t_id = tmk_attr(task, "id", TMK_NS, &err);
    if (err) { json_free(task); json_free(fp); ret = err; goto body_close; }
    json_t *pid = tmk_getpid(ctx);
    if (tmk_is_raise(pid)) { json_free(task); json_free(fp); ret = pid; goto body_close; }
    const json_t *claim2 = tmk_sub(context, "claim_lock", &err);
    if (err) { json_free(task); json_free(fp); json_free(pid); ret = err; goto body_close; }
    const json_t *rid2 = tmk_sub(context, "run_id", &err);
    if (err) { json_free(task); json_free(fp); json_free(pid); ret = err; goto body_close; }
    json_t *payload = json_object();
    tmk_aset(payload, "pid", pid);
    tmk_asets(payload, "claim_lock", tmk_str(claim2));
    tmk_asets(payload, "started_at", fingerprint ? fingerprint : "unverified");
    json_free(pid);
    json_free(fp);
    ret = tmk_append_event(ctx, tmk_str(t_id), "worker_bound", payload, rid2);
    if (tmk_is_raise(ret)) { json_free(task); goto body_close; }
    json_t *cp = json_array();
    json_append(cp, json_copy(t_id));
    json_append(cp, json_copy(rid2));
    json_t *claimed = tmk_execute(ctx,
        "SELECT payload, created_at FROM task_events WHERE task_id=? AND run_id=? "
        "AND kind='claimed' ORDER BY id DESC LIMIT 1", cp);
    json_free(task);
    if (tmk_is_raise(claimed)) { ret = claimed; goto body_close; }
    /* the with block ends here: write_txn exit silent, conn.close fires */
    {
        json_t *cr = tmk_conn_close(ctx);
        if (tmk_is_raise(ret)) {
            json_free(cr);
        } else if (tmk_is_raise(cr)) {
            json_free(ret);
            ret = cr;
        } else {
            json_free(cr);
        }
    }
    if (tmk_is_raise(ret)) goto fail;
    /* expires = json.loads(claimed[0]).get('expires') if claimed else None */
    json_t *expires = NULL;
    json_t *created_at = NULL;
    if (tmk_truthy(claimed)) {
        const json_t *cpayload = json_get(claimed, 0);
        const json_t *ca_v = json_get(claimed, 1);
        if (ca_v) created_at = json_copy(ca_v);
        json_t *cparsed = tmk_loads(tmk_str(cpayload), &err);
        if (err) { json_free(claimed); json_free(created_at); ret = err; goto fail; }
        expires = json_obj_get(cparsed, "expires");
        if (expires) expires = json_copy(expires);
        json_free(cparsed);
    }
    json_free(claimed);
    /* isinstance(expires, int) admits bool (bool is an int subclass) */
    bool expires_is_int = expires &&
        ((expires->type == JSON_NUMBER &&
          expires->num_val == (double)(long long)expires->num_val)
         || expires->type == JSON_BOOL);
    if (expires_is_int && tmk_truthy(created_at)) {
        json_t *ca_int = tmk_pyint(created_at, &err);
        if (err) { json_free(expires); ret = err; goto fail; }
        double ev = expires->type == JSON_BOOL ? (expires->bool_val ? 1 : 0)
                                               : expires->num_val;
        double diff = ev - ca_int->num_val;
        json_free(ca_int);
        double ttl = diff > 1 ? diff : 1;
        char ttl_str[64];
        snprintf(ttl_str, sizeof ttl_str, "%lld", (long long)ttl);
        tmk_asets(env, "HERMES_KANBAN_CLAIM_TTL_SECONDS", ttl_str);
    }
    json_free(expires);
    json_free(created_at);
    /* _watch_submitter(Path(context['db']), context['run_id']) — sibling */
    const json_t *db3 = tmk_sub(context, "db", &err);
    if (err) { ret = err; goto fail; }
    const json_t *rid3 = tmk_sub(context, "run_id", &err);
    if (err) { ret = err; goto fail; }
    json_t *wa = tmk_args0();
    tmk_asets(wa, "db", tmk_str(db3));
    tmk_aset(wa, "run_id", rid3);
    json_t *wr = tmk_call(ctx, "watch_submitter", wa);
    if (tmk_is_raise(wr)) { ret = wr; goto fail; }
    json_free(wr);
    /* os.environ.update(env) */
    for (size_t i = 0; i < env->c.count; i++)
        json_set(env_mirror, env->c.keys[i], json_copy(env->c.items[i]));
    /* os.chdir(context['workspace']) */
    const json_t *ws2 = tmk_sub(context, "workspace", &err);
    if (err) { ret = err; goto fail; }
    json_t *ca = tmk_args0();
    tmk_asets(ca, "path", tmk_str(ws2));
    ret = tmk_call(ctx, "os.chdir", ca);
    goto fail;
body_close:
    ;
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
fail:
    json_free(env);
    json_free(context);
    return ret ? ret : json_null();
}

/* PoP: tmk_watch_submitter @ gateway/session_kanban.py:_watch_submitter */
static json_t *tmk_watch_submitter(tmk_ctx_t *ctx, const json_t *args) {
    json_t *a = tmk_args0();
    tmk_asets(a, "target", "watch");
    tmk_asets(a, "name", "kanban-submitter-watch");
    tmk_asetb(a, "daemon", true);
    json_t *thread = tmk_call(ctx, "threading.Thread", a);
    if (tmk_is_raise(thread)) return thread;
    json_free(thread);
    json_t *sr = tmk_call(ctx, "thread.start", tmk_args0());
    if (tmk_is_raise(sr)) return sr;
    json_free(sr);
    return json_null();
}

/* PoP: tmk_watch @ gateway/session_kanban.py:watch */
static json_t *tmk_watch(tmk_ctx_t *ctx, const json_t *args) {
    const char *db = json_get_str(args, "db", "");
    const json_t *run_id = json_obj_get(args, "run_id");
    json_t *r = tmk_sqlite_connect_ro(ctx, db, true);
    if (tmk_is_raise(r)) return r;
    json_free(r);
    json_t *ret = NULL;
    for (;;) {
        json_t *wa = tmk_args0();
        tmk_asetn(wa, "timeout", 1.0);
        json_t *done = tmk_call(ctx, "turns_done.wait", wa);
        if (tmk_is_raise(done)) { ret = done; break; }
        bool is_done = tmk_truthy(done);
        json_free(done);
        if (is_done) { ret = json_null(); break; }
        json_t *p = json_array();
        json_append(p, json_copy(run_id));
        json_t *row = tmk_execute(ctx,
            "SELECT worker_pid, worker_started_at FROM task_runs WHERE id=?", p);
        if (tmk_is_raise(row)) {
            if (tmk_exc_isa(tmk_raise_kind(row), "sqlite3.Error")) {
                json_free(row);
                continue;
            }
            ret = row;
            break;
        }
        bool fire_exit = false;
        if (tmk_truthy(row)) {
            const json_t *wpid = json_get(row, 0);
            if (tmk_truthy(wpid)) {
                json_t *pid = tmk_getpid(ctx);
                if (tmk_is_raise(pid)) { json_free(row); ret = pid; break; }
                bool same = tmk_json_eq(wpid, pid);
                json_free(pid);
                if (!same) {
                    const json_t *started = json_get(row, 1);
                    json_t *aa = tmk_args0();
                    tmk_aset(aa, "pid", wpid);
                    tmk_aset(aa, "started_at", started);
                    json_t *alive = tmk_call(ctx, "kbd._worker_alive", aa);
                    if (tmk_is_raise(alive)) { json_free(row); ret = alive; break; }
                    fire_exit = !tmk_truthy(alive);
                    json_free(alive);
                }
            }
        }
        json_free(row);
        if (fire_exit) {
            json_t *ea = tmk_args0();
            tmk_asetn(ea, "code", 1);
            json_t *er = tmk_call(ctx, "os._exit", ea);
            if (tmk_is_raise(er)) {
                ret = er;      /* fail-closed seam raise propagates */
            } else {
                json_free(er);
                /* os._exit(1) models as SystemExit(1) — the catchable analog */
                ret = tmk_err_spec("SystemExit", "1");
            }
            break;
        }
    }
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    return ret ? ret : json_null();
}

/* PoP: tmk_run_worker_turns @ gateway/session_kanban.py:run_worker_turns */
static json_t *tmk_run_worker_turns(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *agent = json_obj_get(args, "agent");
    const json_t *frame = json_obj_get(args, "frame");
    const json_t *history = json_obj_get(args, "history");
    (void)agent;
    json_t *a = tmk_args0();
    tmk_aset(a, "frame", frame);
    json_t *scope = tmk_call(ctx, "worker_turn_scope", a);
    if (tmk_is_raise(scope)) return scope;
    json_free(scope);
    json_t *a2 = tmk_args0();
    tmk_asets(a2, "agent", "agent");
    json_t *arm = tmk_call(ctx, "arm_surface_turn", a2);
    if (tmk_is_raise(arm)) return arm;
    json_free(arm);
    json_t *a3 = tmk_args0();
    tmk_asets(a3, "agent", "agent");
    tmk_aset(a3, "frame", frame);
    tmk_aset(a3, "history", history);
    return tmk_call(ctx, "run_worker_turns_inner", a3);
}

/* PoP: tmk_worker_workspace @ gateway/session_kanban.py:_worker_workspace */
static json_t *tmk_worker_workspace(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *frame = json_obj_get(args, "frame");
    const json_t *scope = json_obj_get(frame, "scope");
    const json_t *sid = tmk_truthy(scope) ? json_obj_get(scope, "session_id") : NULL;
    if (!tmk_truthy(sid)) return json_null();   /* yield None */
    json_t *err = NULL;
    const json_t *policy_in = tmk_sub(frame, "policy", &err);
    if (err) return err;
    json_t *pa = tmk_args0();
    tmk_aset(pa, "data", policy_in);
    json_t *policy = tmk_call(ctx, "restore_policy", pa);
    if (tmk_is_raise(policy)) return policy;
    const json_t *route_v = json_obj_get(frame, "route");
    const char *route = tmk_truthy(route_v) ? tmk_str(route_v) : tmk_str(sid);
    json_t *ha = tmk_args0();
    tmk_aset(ha, "home", json_obj_get(frame, "home"));
    json_t *profile = tmk_call(ctx, "served_profile_name", ha);
    if (tmk_is_raise(profile)) { json_free(policy); return profile; }
    json_t *pfa = tmk_args0();
    tmk_asets(pfa, "route", route ? route : "");
    json_t *key_profile = tmk_call(ctx, "profile_from_session_key", pfa);
    if (tmk_is_raise(key_profile)) { json_free(policy); json_free(profile); return key_profile; }
    const char *bind_profile = "";
    if (tmk_json_eq(key_profile, profile)) {
        const char *ps = tmk_str(profile);
        bind_profile = ps ? ps : "";
    }
    json_free(key_profile);
    json_t *sa = tmk_args0();
    tmk_asets(sa, "session_key", route ? route : "");
    tmk_aset(sa, "session_id", sid);
    tmk_asets(sa, "profile", bind_profile);
    json_t *tokens = tmk_call(ctx, "set_session_vars", sa);
    if (tmk_is_raise(tokens)) { json_free(policy); json_free(profile); return tokens; }
    json_t *ba = tmk_args0();
    tmk_aset(ba, "policy", policy);
    tmk_aset(ba, "session_id", sid);
    json_t *br = tmk_call(ctx, "bind_turn_workspace", ba);
    json_free(policy);
    json_free(profile);
    if (tmk_is_raise(br)) {
        /* finally: clear_session_vars(tokens) before the raise propagates */
        json_t *ca = tmk_args0();
        tmk_aset(ca, "tokens", tokens);
        json_t *clr = tmk_call(ctx, "clear_session_vars", ca);
        json_free(clr);
        json_free(tokens);
        return br;
    }
    json_free(br);
    /* yield session_id; the CM case then drives __exit__ -> finally */
    json_t *ca = tmk_args0();
    tmk_aset(ca, "tokens", tokens);
    json_t *clr = tmk_call(ctx, "clear_session_vars", ca);
    json_free(tokens);
    if (tmk_is_raise(clr)) return clr;
    json_free(clr);
    return json_copy(sid);
}

/* PoP: tmk_run_worker_turns_inner @ gateway/session_kanban.py:_run_worker_turns */
static json_t *tmk_run_worker_turns_inner(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *agent = json_obj_get(args, "agent");
    const json_t *frame = json_obj_get(args, "frame");
    const json_t *history = json_obj_get(args, "history");
    (void)agent;
    json_t *err = NULL;
    const json_t *policy = tmk_sub(frame, "policy", &err);
    if (err) return err;
    const json_t *kj = json_obj_get(policy, "kanban_json");
    json_t *context = tmk_loads(tmk_truthy(kj) ? tmk_str(kj) : "null", &err);
    if (err) return err;
    if (!context || context->type == JSON_NULL) {
        json_free(context);
        const json_t *author = json_obj_get(frame, "turn_author");
        /* with _worker_workspace(frame) as task_id — sibling CM seam */
        json_t *wa = tmk_args0();
        tmk_aset(wa, "frame", frame);
        json_t *task_id = tmk_call(ctx, "worker_workspace", wa);
        if (tmk_is_raise(task_id)) return task_id;
        /* frame['text'] evaluates BEFORE worker_display_kwargs() */
        const json_t *text = tmk_sub(frame, "text", &err);
        if (err) { json_free(task_id); return err; }
        json_t *dk = tmk_call(ctx, "worker_display_kwargs", tmk_args0());
        if (tmk_is_raise(dk)) { json_free(task_id); return dk; }
        json_t *a = tmk_args0();
        tmk_asets(a, "text", tmk_str(text));
        tmk_aset(a, "conversation_history", history);
        if (dk && dk->type == JSON_OBJECT)
            for (size_t i = 0; i < dk->c.count; i++)
                json_set(a, dk->c.keys[i], json_copy(dk->c.items[i]));
        json_free(dk);
        if (tmk_truthy(task_id)) tmk_aset(a, "task_id", task_id);
        if (author && author->type != JSON_NULL) tmk_aset(a, "turn_author", author);
        json_free(task_id);
        return tmk_call(ctx, "agent.run_conversation", a);
    }
    json_t *code = json_number(1);
    char *last_output = tmk_strdup("");
    json_t *ret = NULL;
    json_t *a = tmk_args0();
    tmk_asets(a, "agent", "agent");
    tmk_aset(a, "frame", frame);
    tmk_aset(a, "history", history);
    tmk_aset(a, "context", context);
    json_t *result = tmk_call(ctx, "run_task_turns", a);
    if (tmk_is_raise(result)) { ret = result; goto except_or_finally; }
    json_t *ta = tmk_args0();
    tmk_aset(ta, "result", result);
    tmk_asetb(ta, "kanban_worker", true);
    json_t *tc = tmk_call(ctx, "turn_exit_code", ta);
    if (tmk_is_raise(tc)) { json_free(result); ret = tc; goto except_or_finally; }
    json_free(code);
    code = tc;
    if (result && result->type == JSON_OBJECT) {
        const json_t *fr = json_obj_get(result, "final_response");
        char *s = tmk_truthy(fr) ? tmk_pystr(fr) : tmk_strdup("");
        json_t *ra = tmk_args0();
        tmk_asets(ra, "value", s);
        free(s);
        json_t *redacted = tmk_call(ctx, "kb.redact_review_value", ra);
        if (tmk_is_raise(redacted)) {
            json_free(result);
            ret = redacted;
            goto except_or_finally;
        }
        free(last_output);
        last_output = tmk_slice_tail(tmk_str(redacted), 500);
        json_free(redacted);
    }
    ret = result;   /* return result — finally fires first */
    goto finally;
except_or_finally:
    if (!tmk_is_base_exc(tmk_raise_kind(ret))) {
        /* except Exception as exc: code = turn_exit_code(None, **flags); raise */
        json_t *fa = tmk_args0();
        json_set(fa, "exc", json_copy(ret));
        json_t *flags = tmk_call(ctx, "credential_failure_flags", fa);
        if (tmk_is_raise(flags)) {
            json_free(ret);
            ret = flags;
        } else {
            json_t *ta = tmk_args0();
            tmk_aset(ta, "result", NULL);   /* None — dropped */
            tmk_asetb(ta, "kanban_worker", true);
            if (flags && flags->type == JSON_OBJECT)
                for (size_t i = 0; i < flags->c.count; i++)
                    json_set(ta, flags->c.keys[i], json_copy(flags->c.items[i]));
            json_free(flags);
            json_t *tc = tmk_call(ctx, "turn_exit_code", ta);
            if (tmk_is_raise(tc)) {
                json_free(ret);
                ret = tc;
            } else {
                json_free(code);
                code = tc;
            }
        }
    }
finally:
    ;
    json_t *fargs = tmk_args0();
    tmk_aset(fargs, "context", context);
    tmk_aset(fargs, "code", code);
    tmk_asets(fargs, "last_output", last_output);
    json_t *fr = tmk_call(ctx, "record_worker_result", fargs);
    if (tmk_is_raise(fr)) {
        json_free(ret);
        ret = fr;
    } else {
        json_free(fr);
    }
    json_free(context);
    json_free(code);
    free(last_output);
    return ret ? ret : json_null();
}/* ── the kanban admission engine ── */

/* KANBAN_TURN_AUTHOR (module constant, byte-pinned). */
static json_t *tmk_turn_author(void) {
    json_t *o = json_object();
    tmk_asets(o, "id", "kanban:dispatcher");
    tmk_asets(o, "name", "Kanban dispatcher");
    tmk_asetb(o, "is_bot", true);
    return o;
}

/* 'kanban:' + json.dumps([board, task_id, run_id], separators=(',', ':')) */
static char *tmk_request_id(const json_t *board, const json_t *task_id,
                            const json_t *run_id) {
    json_t *arr = json_array();
    json_append(arr, board ? json_copy(board) : json_null());
    json_append(arr, task_id ? json_copy(task_id) : json_null());
    json_append(arr, run_id ? json_copy(run_id) : json_null());
    char *d = tmk_dumps(arr, false, true);
    json_free(arr);
    char *o = malloc(strlen(d) + 16);
    if (o) sprintf(o, "kanban:%s", d);
    free(d);
    return o;
}

/* PoP: tmk_build_kanban_policy @ gateway/session_kanban.py:build_kanban_policy */
static json_t *tmk_build_kanban_policy(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *connection = json_obj_get(args, "connection");
    const json_t *params = json_obj_get(args, "params");
    const json_t *config = json_obj_get(args, "config");
    json_t *err = NULL;
    const json_t *authority = tmk_attr(connection, "authority", TMK_NS, &err);
    if (err) return err;
    const json_t *actor = tmk_attr(connection, "actor", TMK_NS, &err);
    if (err) return err;
    const json_t *native = tmk_attr(connection, "native_owner", TMK_NS, &err);
    if (err) return err;
    if (!tmk_truthy(native))
        return tmk_err_spec("AuthorityRefusal", "permission_denied");
    const json_t *caps = tmk_attr(actor, "capabilities", TMK_NS, &err);
    if (err) return err;
    if (!tmk_in_list(caps, "session:create"))
        return tmk_err_spec("AuthorityRefusal", "permission_denied");
    const json_t *a_pid = tmk_attr(actor, "profile_id", TMK_NS, &err);
    if (err) return err;
    const json_t *auth_pid = tmk_attr(authority, "profile_id", TMK_NS, &err);
    if (err) return err;
    if (!tmk_json_eq(a_pid, auth_pid))
        return tmk_err_spec("AuthorityRefusal", "profile_mismatch");
    static const char *KEYS4[] = {"board", "task_id", "run_id", "claim_lock"};
    static const char *KEYS5[] = {"board", "task_id", "run_id", "claim_lock", "db"};
    bool keyset_ok = tmk_keyset_is(params, KEYS4, 4) || tmk_keyset_is(params, KEYS5, 5);
    bool params_ok = keyset_ok;
    if (params_ok) {
        static const char *STRKEYS[] = {"board", "task_id", "claim_lock"};
        for (int i = 0; i < 3; i++) {
            const json_t *v = tmk_sub(params, STRKEYS[i], &err);
            if (err) return err;
            if (!v || v->type != JSON_STRING || !tmk_truthy(v)) { params_ok = false; break; }
        }
    }
    if (params_ok) {
        const json_t *rid = tmk_sub(params, "run_id", &err);
        if (err) return err;
        /* type(run_id) is int — bool/float/str are NOT int by type identity */
        if (!rid || rid->type != JSON_NUMBER ||
            rid->num_val != (double)(long long)rid->num_val)
            params_ok = false;
    }
    if (!params_ok)
        return tmk_err_spec("AuthorityRefusal", "invalid_params");

    /* try: ... except (ValueError, OSError, sqlite3.Error) -> invalid_kanban_claim */
    json_t *ret = NULL;
    json_t *task = NULL;
    char *board = NULL;
    char *path = NULL;
    json_t *skills = NULL;
    char *context_str = NULL;
    bool in_try = true;
    const json_t *board_v = tmk_sub(params, "board", &err);
    if (err) { ret = err; goto try_end; }
    json_t *sa = tmk_args0();
    tmk_asets(sa, "slug", tmk_str(board_v));
    json_t *board_r = tmk_call(ctx, "kb._require_slug", sa);
    if (tmk_is_raise(board_r)) { ret = board_r; goto try_end; }
    board = tmk_strdup(tmk_str(board_r));
    json_free(board_r);
    if (json_obj_get(params, "db")) {
        const json_t *db_v = tmk_sub(params, "db", &err);
        if (err) { ret = err; goto try_end; }
        json_t *pr = tmk_path_resolve(ctx, tmk_str(db_v), true);
        if (tmk_is_raise(pr)) { ret = pr; goto try_end; }
        path = tmk_strdup(tmk_str(pr));
        json_free(pr);
    } else {
        json_t *ka = tmk_args0();
        tmk_asets(ka, "board", board);
        json_t *kp = tmk_call(ctx, "kb.kanban_db_path", ka);
        if (tmk_is_raise(kp)) { ret = kp; goto try_end; }
        json_t *pr = tmk_path_resolve(ctx, tmk_str(kp), false);
        json_free(kp);
        if (tmk_is_raise(pr)) { ret = pr; goto try_end; }
        path = tmk_strdup(tmk_str(pr));
        json_free(pr);
    }
    /* with closing(sqlite3.connect(path.as_uri() + '?mode=ro', uri=True)) */
    {
        json_t *ua = tmk_args0();
        tmk_asets(ua, "path", path);
        json_t *uri = tmk_call(ctx, "path.as_uri", ua);
        if (tmk_is_raise(uri)) { ret = uri; goto try_end; }
        const char *us = tmk_str(uri);
        char *full = malloc((us ? strlen(us) : 0) + 16);
        if (full) sprintf(full, "%s?mode=ro", us ? us : "");
        json_t *ca = tmk_args0();
        tmk_asets(ca, "path", full ? full : "?mode=ro");
        tmk_asetb(ca, "uri", true);
        json_t *conn = tmk_call(ctx, "sqlite3.connect", ca);
        free(full);
        json_free(uri);
        if (tmk_is_raise(conn)) { ret = conn; goto try_end; }
        json_free(conn);
    }
    /* conn.row_factory = sqlite3.Row — silent attribute set */
    const json_t *tid_v = tmk_sub(params, "task_id", &err);
    if (err) { ret = err; goto conn_close; }
    json_t *ga = tmk_args0();
    tmk_asets(ga, "conn", "conn");
    tmk_asets(ga, "task_id", tmk_str(tid_v));
    task = tmk_call(ctx, "kb.get_task", ga);
    if (tmk_is_raise(task)) { ret = task; task = NULL; goto conn_close; }
    const json_t *rid_v = tmk_sub(params, "run_id", &err);
    if (err) { ret = err; goto conn_close; }
    json_t *rp = json_array();
    json_append(rp, json_copy(rid_v));
    json_t *run = tmk_execute(ctx, "SELECT * FROM task_runs WHERE id=?", rp);
    if (tmk_is_raise(run)) { ret = run; goto conn_close; }
    /* the stale-claim check chain (short-circuit, live read order) */
    {
        bool stale = false;
        const json_t *t_status = NULL, *t_crid = NULL, *t_claim = NULL, *t_id = NULL;
        if (!task || task->type == JSON_NULL) {
            stale = true;
        } else if (!run || run->type == JSON_NULL) {
            stale = true;
        } else {
            t_status = tmk_attr(task, "status", TMK_NS, &err);
            if (err) { json_free(run); ret = err; goto conn_close; }
            const char *ts = tmk_str(t_status);
            if (!ts || strcmp(ts, "running") != 0) {
                stale = true;
            } else {
                const json_t *r_status = tmk_sub(run, "status", &err);
                if (err) { json_free(run); ret = err; goto conn_close; }
                const char *rs = tmk_str(r_status);
                if (!rs || strcmp(rs, "running") != 0) {
                    stale = true;
                } else {
                    t_crid = tmk_attr(task, "current_run_id", TMK_NS, &err);
                    if (err) { json_free(run); ret = err; goto conn_close; }
                    const json_t *p_rid = tmk_sub(params, "run_id", &err);
                    if (err) { json_free(run); ret = err; goto conn_close; }
                    if (!tmk_json_eq(t_crid, p_rid)) {
                        stale = true;
                    } else {
                        const json_t *r_tid = tmk_sub(run, "task_id", &err);
                        if (err) { json_free(run); ret = err; goto conn_close; }
                        t_id = tmk_attr(task, "id", TMK_NS, &err);
                        if (err) { json_free(run); ret = err; goto conn_close; }
                        if (!tmk_json_eq(r_tid, t_id)) {
                            stale = true;
                        } else {
                            t_claim = tmk_attr(task, "claim_lock", TMK_NS, &err);
                            if (err) { json_free(run); ret = err; goto conn_close; }
                            const json_t *p_claim = tmk_sub(params, "claim_lock", &err);
                            if (err) { json_free(run); ret = err; goto conn_close; }
                            if (!tmk_json_eq(t_claim, p_claim)) {
                                stale = true;
                            } else {
                                const json_t *r_claim = tmk_sub(run, "claim_lock", &err);
                                if (err) { json_free(run); ret = err; goto conn_close; }
                                const json_t *t_claim2 = tmk_attr(task, "claim_lock", TMK_NS, &err);
                                if (err) { json_free(run); ret = err; goto conn_close; }
                                if (!tmk_json_eq(r_claim, t_claim2)) stale = true;
                            }
                        }
                    }
                }
            }
        }
        json_free(run);
        if (stale) {
            ret = tmk_err_spec("AuthorityRefusal", "stale_kanban_claim");
            goto conn_close;
        }
        /* profile match: resolve_profile_env(task.assignee) vs authority.profile_id */
        const json_t *assignee = tmk_attr(task, "assignee", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        json_t *ea = tmk_args0();
        tmk_asets(ea, "profile", tmk_str(assignee));
        json_t *env_path = tmk_call(ctx, "resolve_profile_env", ea);
        if (tmk_is_raise(env_path)) { ret = env_path; goto conn_close; }
        json_t *r1 = tmk_path_resolve(ctx, tmk_str(env_path), false);
        json_free(env_path);
        if (tmk_is_raise(r1)) { ret = r1; goto conn_close; }
        const json_t *auth_pid2 = tmk_attr(authority, "profile_id", TMK_NS, &err);
        if (err) { json_free(r1); ret = err; goto conn_close; }
        json_t *r2 = tmk_path_resolve(ctx, tmk_str(auth_pid2), false);
        if (tmk_is_raise(r2)) { json_free(r1); ret = r2; goto conn_close; }
        bool mismatch = !tmk_json_eq(r1, r2);
        json_free(r1);
        json_free(r2);
        if (mismatch) {
            ret = tmk_err_spec("AuthorityRefusal", "profile_mismatch");
            goto conn_close;
        }
        /* bound owner_admitted marker */
        const json_t *t_id2 = tmk_attr(task, "id", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        const json_t *t_crid2 = tmk_attr(task, "current_run_id", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        json_t *bp = json_array();
        json_append(bp, json_copy(t_id2));
        json_append(bp, json_copy(t_crid2));
        json_t *bound = tmk_execute(ctx,
            "SELECT payload FROM task_events WHERE task_id=? AND run_id=? "
            "AND kind='owner_admitted' ORDER BY id DESC LIMIT 1", bp);
        if (tmk_is_raise(bound)) { ret = bound; goto conn_close; }
        if (tmk_truthy(bound)) {
            const json_t *payload = json_get(bound, 0);
            json_t *parsed = tmk_loads(tmk_str(payload), &err);
            if (err) { json_free(bound); ret = err; goto conn_close; }
            const json_t *req = tmk_sub(parsed, "request_id", &err);
            if (err) { json_free(bound); json_free(parsed); ret = err; goto conn_close; }
            const json_t *t_id3 = tmk_attr(task, "id", TMK_NS, &err);
            if (err) { json_free(bound); json_free(parsed); ret = err; goto conn_close; }
            const json_t *t_crid3 = tmk_attr(task, "current_run_id", TMK_NS, &err);
            if (err) { json_free(bound); json_free(parsed); ret = err; goto conn_close; }
            json_t *want_arr = json_array();
            json_append(want_arr, json_string(board));
            json_append(want_arr, json_copy(t_id3));
            json_append(want_arr, json_copy(t_crid3));
            char *want_d = tmk_dumps(want_arr, false, true);
            json_free(want_arr);
            char *want = malloc(strlen(want_d) + 16);
            if (want) sprintf(want, "kanban:%s", want_d);
            free(want_d);
            const char *req_s = tmk_str(req);
            bool stale_bound = !req_s || !want || strcmp(req_s, want) != 0;
            free(want);
            json_free(parsed);
            if (stale_bound) {
                json_free(bound);
                ret = tmk_err_spec("AuthorityRefusal", "stale_kanban_claim");
                goto conn_close;
            }
        }
        json_free(bound);
        /* context = kb.build_worker_context(conn, task.id) */
        const json_t *t_id4 = tmk_attr(task, "id", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        json_t *bwa = tmk_args0();
        tmk_asets(bwa, "conn", "conn");
        tmk_asets(bwa, "task_id", tmk_str(t_id4));
        json_t *bwc = tmk_call(ctx, "kb.build_worker_context", bwa);
        if (tmk_is_raise(bwc)) { ret = bwc; goto conn_close; }
        context_str = tmk_strdup(tmk_str(bwc));
        json_free(bwc);
        /* skills = list(task.skills or ()) */
        const json_t *t_skills = tmk_attr(task, "skills", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        skills = json_array();
        if (tmk_truthy(t_skills) && t_skills->type == JSON_ARRAY)
            for (size_t i = 0; i < t_skills->c.count; i++)
                json_append(skills, json_copy(t_skills->c.items[i]));
        /* retry review lane loads sdlc-review */
        const json_t *t_id5 = tmk_attr(task, "id", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        const json_t *t_crid5 = tmk_attr(task, "current_run_id", TMK_NS, &err);
        if (err) { ret = err; goto conn_close; }
        json_t *rsa = tmk_args0();
        tmk_asets(rsa, "conn", "conn");
        tmk_asets(rsa, "task_id", tmk_str(t_id5));
        tmk_aset(rsa, "run_id", t_crid5);
        json_t *retry = tmk_call(ctx, "kb._retry_status_for_run", rsa);
        if (tmk_is_raise(retry)) { ret = retry; goto conn_close; }
        const char *retry_s = tmk_str(retry);
        if (retry_s && !strcmp(retry_s, "review")) {
            json_append(skills, json_string("sdlc-review"));
            json_t *dd = tmk_dedup(skills);
            json_free(skills);
            skills = dd;
        }
        json_free(retry);
    }
conn_close:
    ;
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        ret = cr;
    } else {
        json_free(cr);
    }
try_end:
    if (tmk_is_raise(ret)) {
        const char *kind = tmk_raise_kind(ret);
        if (tmk_exc_isa(kind, "ValueError") || tmk_exc_isa(kind, "OSError") ||
            tmk_exc_isa(kind, "sqlite3.Error")) {
            json_free(ret);
            ret = tmk_err_spec("AuthorityRefusal", "invalid_kanban_claim");
        }
        in_try = false;
        goto cleanup;
    }
    in_try = false;
    /* cwd = task.workspace_path; params_policy composition */
    {
        const json_t *cwd = tmk_attr(task, "workspace_path", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        json_t *pp = json_object();
        tmk_asets(pp, "source", "cli");
        tmk_asets(pp, "cwd", tmk_str(cwd));
        static const char *names[] = {"model", "provider", "reasoning"};
        static const char *fields[] = {"model_override", "provider_override", "reasoning_effort"};
        for (int i = 0; i < 3; i++) {
            const json_t *v = tmk_attr(task, fields[i], TMK_NS, &err);
            if (err) { json_free(pp); ret = err; goto cleanup; }
            if (tmk_truthy(v)) tmk_asets(pp, names[i], tmk_str(v));
        }
        json_t *private = json_object();
        json_t *bpa = tmk_args0();
        tmk_aset(bpa, "params", pp);
        tmk_aset(bpa, "config", config);
        tmk_aset(bpa, "private_secrets", private);
        json_t *policy = tmk_call(ctx, "build_policy", bpa);
        json_free(pp);
        if (tmk_is_raise(policy)) { json_free(private); ret = policy; goto cleanup; }
        /* if policy.model is None: replace(model=_resolve_gateway_model(policy.config())) */
        const json_t *model = tmk_attr(policy, "model", "Policy", &err);
        if (err) { json_free(policy); json_free(private); ret = err; goto cleanup; }
        if (!model || model->type == JSON_NULL) {
            json_t *cfg = tmk_call(ctx, "policy.config", tmk_args0());
            if (tmk_is_raise(cfg)) {
                json_free(policy); json_free(private); ret = cfg; goto cleanup;
            }
            json_t *rma = tmk_args0();
            tmk_aset(rma, "config", cfg);
            json_t *gm = tmk_call(ctx, "resolve_gateway_model", rma);
            json_free(cfg);
            if (tmk_is_raise(gm)) {
                json_free(policy); json_free(private); ret = gm; goto cleanup;
            }
            json_set(policy, "model", gm);
        }
        /* context = dict(params, workspace=..., ...) */
        json_t *kctx = params ? json_copy(params) : json_object();
        tmk_asets(kctx, "workspace", tmk_str(cwd));
        const json_t *branch = tmk_attr(task, "branch_name", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "branch", branch);
        const json_t *tenant = tmk_attr(task, "tenant", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "tenant", tenant);
        const json_t *assignee2 = tmk_attr(task, "assignee", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "profile", assignee2);
        json_t *wra = tmk_args0();
        tmk_asets(wra, "board", board);
        json_t *wr = tmk_call(ctx, "kb.workspaces_root", wra);
        if (tmk_is_raise(wr)) {
            json_free(kctx); json_free(policy); json_free(private); ret = wr; goto cleanup;
        }
        json_t *wrr = tmk_path_resolve(ctx, tmk_str(wr), false);
        json_free(wr);
        if (tmk_is_raise(wrr)) {
            json_free(kctx); json_free(policy); json_free(private); ret = wrr; goto cleanup;
        }
        tmk_asets(kctx, "workspaces_root", tmk_str(wrr));
        json_free(wrr);
        tmk_aset(kctx, "skills", skills);
        skills = NULL;   /* adopted */
        const json_t *goal_mode = tmk_attr(task, "goal_mode", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "goal_mode", goal_mode);
        const json_t *gmt = tmk_attr(task, "goal_max_turns", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "goal_max_turns", gmt);
        const json_t *title = tmk_attr(task, "title", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        const json_t *body = tmk_attr(task, "body", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_sbuf_t gt;
        tmk_sb_init(&gt);
        bool first = true;
        const json_t *parts[2] = {title, body};
        for (int i = 0; i < 2; i++) {
            if (tmk_truthy(parts[i])) {
                if (!first) tmk_sb_puts(&gt, "\n\n");
                tmk_sb_puts(&gt, tmk_str(parts[i]));
                first = false;
            }
        }
        json_set(kctx, "goal_text", json_string(gt.buf ? gt.buf : ""));
        free(gt.buf);
        tmk_asets(kctx, "context", context_str);
        const json_t *mrs = tmk_attr(task, "max_runtime_seconds", TMK_NS, &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(kctx, "max_runtime_seconds", mrs);
        tmk_asetb(kctx, "accept_hooks", true);
        tmk_asets(kctx, "db", path);
        /* request = json.loads(policy.request_json); request.update(...) */
        const json_t *rj = tmk_attr(policy, "request_json", "Policy", &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        json_t *request = tmk_loads(tmk_str(rj), &err);
        if (err) { json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_asets(request, "source", "kanban");
        tmk_asets(request, "board", board);
        const json_t *t_id6 = tmk_attr(task, "id", TMK_NS, &err);
        if (err) { json_free(request); json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(request, "task_id", t_id6);
        const json_t *t_crid6 = tmk_attr(task, "current_run_id", TMK_NS, &err);
        if (err) { json_free(request); json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        tmk_aset(request, "run_id", t_crid6);
        /* replace(policy, source=, platform=, toolsets=, request_json=, kanban_json=) */
        tmk_asets(policy, "source", "kanban");
        tmk_asets(policy, "platform", "cli");
        const json_t *toolsets = tmk_attr(policy, "toolsets", "Policy", &err);
        if (err) { json_free(request); json_free(kctx); json_free(policy); json_free(private); ret = err; goto cleanup; }
        json_t *ts = tmk_sorted_union(toolsets, "kanban");
        json_set(policy, "toolsets", ts);
        char *rjd = tmk_dumps(request, true, false);
        json_set(policy, "request_json", json_string(rjd));
        free(rjd);
        json_free(request);
        char *kjd = tmk_dumps(kctx, false, false);
        json_set(policy, "kanban_json", json_string(kjd));
        free(kjd);
        json_free(kctx);
        ret = json_array();
        json_append(ret, policy);
        json_append(ret, private);
    }
cleanup:
    (void)in_try;
    json_free(task);
    json_free(skills);
    free(board);
    free(path);
    free(context_str);
    return ret ? ret : json_null();
}

/* PoP: tmk_admit_claim @ gateway/session_kanban.py:admit_claim */
static json_t *tmk_admit_claim(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    const json_t *params = json_obj_get(args, "params");
    const json_t *ref = json_obj_get(args, "ref");
    const json_t *owner_db = json_obj_get(args, "owner_db");
    const json_t *request_id = json_obj_get(args, "request_id");
    json_t *err = NULL;
    const json_t *db = tmk_sub(context, "db", &err);
    if (err) return err;
    json_t *r = tmk_connect_closing(ctx, tmk_str(db));
    if (tmk_is_raise(r)) return r;
    json_free(r);
    r = tmk_write_txn(ctx);
    if (tmk_is_raise(r)) {
        json_t *cr = tmk_conn_close(ctx);
        json_free(cr);
        return r;
    }
    json_free(r);
    json_t *ret = NULL;
    const json_t *tid = tmk_sub(params, "task_id", &err);
    if (err) { ret = err; goto body_close; }
    const json_t *rid = tmk_sub(params, "run_id", &err);
    if (err) { ret = err; goto body_close; }
    json_t *p = json_array();
    json_append(p, json_copy(tid));
    json_append(p, json_copy(rid));
    json_t *marker = tmk_execute(ctx,
        "SELECT payload FROM task_events WHERE task_id=? AND run_id=? "
        "AND kind='owner_admitted' ORDER BY id DESC LIMIT 1", p);
    if (tmk_is_raise(marker)) { ret = marker; goto body_close; }
    if (tmk_truthy(marker)) {
        const json_t *payload = json_get(marker, 0);
        json_t *parsed = tmk_loads(tmk_str(payload), &err);
        if (err) { json_free(marker); ret = err; goto body_close; }
        const json_t *msid = tmk_sub(parsed, "session_id", &err);
        if (err) { json_free(marker); json_free(parsed); ret = err; goto body_close; }
        const json_t *rsid = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { json_free(marker); json_free(parsed); ret = err; goto body_close; }
        bool stale = !tmk_json_eq(msid, rsid);
        json_free(parsed);
        json_free(marker);
        if (stale) {
            ret = tmk_err_spec("AuthorityRefusal", "stale_kanban_claim");
            goto body_close;
        }
        ret = json_null();
    } else {
        json_free(marker);
        const json_t *tid2 = tmk_sub(params, "task_id", &err);
        if (err) { ret = err; goto body_close; }
        json_t *ga = tmk_args0();
        tmk_asets(ga, "conn", "conn");
        tmk_asets(ga, "task_id", tmk_str(tid2));
        json_t *task = tmk_call(ctx, "kb.get_task", ga);
        if (tmk_is_raise(task)) { ret = task; goto body_close; }
        bool ok = tmk_truthy(task);
        const json_t *t_id = NULL;
        const json_t *t_crid = NULL;
        if (ok) {
            const json_t *status = tmk_attr(task, "status", TMK_NS, &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            const char *st = tmk_str(status);
            ok = st && !strcmp(st, "running");
        }
        if (ok) {
            t_crid = tmk_attr(task, "current_run_id", TMK_NS, &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            const json_t *p_rid = tmk_sub(params, "run_id", &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            ok = tmk_json_eq(t_crid, p_rid);
        }
        if (ok) {
            const json_t *t_claim = tmk_attr(task, "claim_lock", TMK_NS, &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            const json_t *p_claim = tmk_sub(params, "claim_lock", &err);
            if (err) { json_free(task); ret = err; goto body_close; }
            ok = tmk_json_eq(t_claim, p_claim);
        }
        if (!ok) {
            json_free(task);
            ret = tmk_err_spec("AuthorityRefusal", "stale_kanban_claim");
            goto body_close;
        }
        /* _append_event(conn, task.id, 'owner_admitted', {db, session_id,
         * request_id, pid: os.getpid(), birth: worker_birth(psutil.Process())},
         * run_id=task.current_run_id) */
        t_id = tmk_attr(task, "id", TMK_NS, &err);
        if (err) { json_free(task); ret = err; goto body_close; }
        const json_t *rsid2 = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { json_free(task); ret = err; goto body_close; }
        json_t *pid = tmk_getpid(ctx);
        if (tmk_is_raise(pid)) { json_free(task); ret = pid; goto body_close; }
        json_t *proc = tmk_call(ctx, "psutil.Process", tmk_args0());
        if (tmk_is_raise(proc)) { json_free(task); json_free(pid); ret = proc; goto body_close; }
        json_free(proc);
        json_t *ba = tmk_args0();
        tmk_asets(ba, "process", "process");
        json_t *birth = tmk_call(ctx, "worker_birth", ba);
        if (tmk_is_raise(birth)) { json_free(task); json_free(pid); ret = birth; goto body_close; }
        const json_t *t_crid2 = tmk_attr(task, "current_run_id", TMK_NS, &err);
        if (err) { json_free(task); json_free(pid); json_free(birth); ret = err; goto body_close; }
        json_t *payload = json_object();
        tmk_aset(payload, "db", owner_db);
        tmk_aset(payload, "session_id", rsid2);
        tmk_aset(payload, "request_id", request_id);
        tmk_aset(payload, "pid", pid);
        tmk_aset(payload, "birth", birth);
        json_free(pid);
        json_free(birth);
        ret = tmk_append_event(ctx, tmk_str(t_id), "owner_admitted", payload, t_crid2);
        json_free(task);
    }
body_close:
    ;
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    return ret ? ret : json_null();
}/* PoP: tmk_run_task @ gateway/session_kanban.py:run_task */
static json_t *tmk_run_task(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *connection = json_obj_get(args, "connection");
    const json_t *params = json_obj_get(args, "params");
    /* cleanup is reachable by goto from before the original declaration;
     * hoist so it is always initialised before any jump. */
    json_t *committed = NULL;
    json_t *err = NULL;
    const json_t *native = tmk_attr(connection, "native_owner", TMK_NS, &err);
    if (err) return err;
    if (!tmk_truthy(native))
        return tmk_err_spec("AuthorityRefusal", "permission_denied");
    const json_t *actor = tmk_attr(connection, "actor", TMK_NS, &err);
    if (err) return err;
    const json_t *caps = tmk_attr(actor, "capabilities", TMK_NS, &err);
    if (err) return err;
    if (!tmk_in_list(caps, "session:create"))
        return tmk_err_spec("AuthorityRefusal", "permission_denied");
    const json_t *authority = tmk_attr(connection, "authority", TMK_NS, &err);
    if (err) return err;
    json_t *apa = tmk_args0();
    tmk_asets(apa, "authority", "authority");
    json_t *store = tmk_call(ctx, "authority_persistence", apa);
    if (tmk_is_raise(store)) return store;
    /* owner_db = str(await to_thread((store_location(scope) or scope.profile_dir).resolve)) */
    const json_t *scope = tmk_attr(store, "scope", TMK_NS, &err);
    if (err) { json_free(store); return err; }
    json_t *sla = tmk_args0();
    tmk_asets(sla, "scope", "scope");
    json_t *loc = tmk_call(ctx, "store_location", sla);
    if (tmk_is_raise(loc)) { json_free(store); return loc; }
    const json_t *profile_dir = NULL;
    if (!tmk_truthy(loc)) {
        profile_dir = tmk_attr(scope, "profile_dir", TMK_NS, &err);
        if (err) { json_free(store); json_free(loc); return err; }
    }
    (void)profile_dir;
    json_t *tta = tmk_args0();
    tmk_asets(tta, "fn", "resolve");
    json_t *owner_db = tmk_call(ctx, "asyncio.to_thread", tta);
    json_free(loc);
    if (tmk_is_raise(owner_db)) { json_free(store); return owner_db; }
    /* request_id = 'kanban:' + json.dumps([params.get(...)...], compact) */
    char *request_id = tmk_request_id(json_obj_get(params, "board"),
                                      json_obj_get(params, "task_id"),
                                      json_obj_get(params, "run_id"));
    const json_t *auth_pid = tmk_attr(authority, "profile_id", TMK_NS, &err);
    if (err) { json_free(store); json_free(owner_db); free(request_id); return err; }
    const json_t *actor2 = tmk_attr(connection, "actor", TMK_NS, &err);
    if (err) { json_free(store); json_free(owner_db); free(request_id); return err; }
    const json_t *subject = tmk_attr(actor2, "subject", TMK_NS, &err);
    if (err) { json_free(store); json_free(owner_db); free(request_id); return err; }
    json_t *lsa = tmk_args0();
    tmk_asets(lsa, "profile_id", tmk_str(auth_pid));
    tmk_asets(lsa, "subject", tmk_str(subject));
    tmk_asets(lsa, "request_id", request_id);
    json_t *sid_r = tmk_call(ctx, "local_session_id", lsa);
    if (tmk_is_raise(sid_r)) {
        json_free(store); json_free(owner_db); free(request_id); return sid_r;
    }
    char *sid = tmk_strdup(tmk_str(sid_r));
    json_free(sid_r);
    /* saved = store.bindings.receipt(sid) if store.sessions.get(sid) is not None */
    const json_t *sessions = tmk_attr(store, "sessions", TMK_NS, &err);
    if (err) { json_free(store); json_free(owner_db); free(request_id); free(sid); return err; }
    const json_t *sess_entry = json_obj_get(sessions, sid);
    json_t *saved = NULL;
    if (sess_entry && sess_entry->type != JSON_NULL) {
        const json_t *bindings = tmk_attr(store, "bindings", TMK_NS, &err);
        if (err) {
            json_free(store); json_free(owner_db); free(request_id); free(sid); return err;
        }
        (void)bindings;
        json_t *ra = tmk_args0();
        tmk_asets(ra, "sid", sid);
        saved = tmk_call(ctx, "store.bindings.receipt", ra);
        if (tmk_is_raise(saved)) {
            json_free(store); json_free(owner_db); free(request_id); free(sid); return saved;
        }
    }
    json_t *policy = NULL;
    json_t *ref = NULL;
    json_t *ret = NULL;
    if (saved && saved->type != JSON_NULL) {
        const json_t *saved_policy = tmk_attr(saved, "policy", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        json_t *rpa = tmk_args0();
        tmk_aset(rpa, "data", saved_policy);
        policy = tmk_call(ctx, "restore_policy", rpa);
        if (tmk_is_raise(policy)) { ret = policy; policy = NULL; goto cleanup; }
        const json_t *kj = tmk_attr(policy, "kanban_json", "Policy", &err);
        if (err) { ret = err; goto cleanup; }
        json_t *sctx = tmk_loads(tmk_truthy(kj) ? tmk_str(kj) : "{}", &err);
        if (err) { ret = err; goto cleanup; }
        /* re-check native_owner, key set and params-vs-context equality */
        const json_t *native2 = tmk_attr(connection, "native_owner", TMK_NS, &err);
        if (err) { json_free(sctx); ret = err; goto cleanup; }
        bool denied = !tmk_truthy(native2);
        static const char *K4[] = {"board", "task_id", "run_id", "claim_lock"};
        static const char *K5[] = {"board", "task_id", "run_id", "claim_lock", "db"};
        if (!denied && !(tmk_keyset_is(params, K4, 4) || tmk_keyset_is(params, K5, 5)))
            denied = true;
        if (!denied && params && params->type == JSON_OBJECT) {
            for (size_t i = 0; i < params->c.count; i++) {
                const json_t *pv = tmk_sub(params, params->c.keys[i], &err);
                if (err) { json_free(sctx); ret = err; goto cleanup; }
                const json_t *cv = json_obj_get(sctx, params->c.keys[i]);
                if (!tmk_json_eq(pv, cv)) { denied = true; break; }
            }
        }
        json_free(sctx);
        if (denied) {
            ret = tmk_err_spec("AuthorityRefusal", "permission_denied");
            goto cleanup;
        }
        json_t *rla = tmk_args0();
        tmk_asets(rla, "authority", "authority");
        tmk_asets(rla, "sid", sid);
        ref = tmk_call(ctx, "restore_local_session", rla);
        if (tmk_is_raise(ref)) { ret = ref; ref = NULL; goto cleanup; }
    } else {
        json_t *cfg = tmk_call(ctx, "load_gateway_config", tmk_args0());
        if (tmk_is_raise(cfg)) { ret = cfg; goto cleanup; }
        json_t *bpa = tmk_args0();
        tmk_asets(bpa, "connection", "connection");
        tmk_aset(bpa, "params", params);
        tmk_aset(bpa, "config", cfg);
        json_t *bp = tmk_call(ctx, "build_kanban_policy", bpa);
        json_free(cfg);
        if (tmk_is_raise(bp)) { ret = bp; goto cleanup; }
        /* policy, private = build_kanban_policy(...) */
        policy = json_copy(json_get(bp, 0));
        json_t *private = json_copy(json_get(bp, 1));
        json_free(bp);
        const json_t *actor3 = tmk_attr(connection, "actor", TMK_NS, &err);
        if (err) { json_free(private); ret = err; goto cleanup; }
        json_t *meta = json_object();
        tmk_asets(meta, "request_id", request_id);
        json_t *cla = tmk_args0();
        tmk_asets(cla, "authority", "authority");
        tmk_aset(cla, "actor", actor3);
        tmk_aset(cla, "params", meta);
        tmk_aset(cla, "trusted_policy", policy);
        tmk_aset(cla, "trusted_secrets", private);
        json_free(private);
        ref = tmk_call(ctx, "create_local_session", cla);
        if (tmk_is_raise(ref)) { ret = ref; ref = NULL; goto cleanup; }
    }
    /* context = json.loads(policy.kanban_json) */
    {
        const json_t *kj2 = tmk_attr(policy, "kanban_json", "Policy", &err);
        if (err) { ret = err; goto cleanup; }
        json_t *context = tmk_loads(tmk_str(kj2), &err);
        if (err) { ret = err; goto cleanup; }
        /* await tracked_write(authority, admit_claim) — records, then runs */
        json_t *twa = tmk_args0();
        tmk_asets(twa, "authority", "authority");
        tmk_asets(twa, "fn", "admit_claim");
        json_t *tw = tmk_call(ctx, "tracked_write", twa);
        if (tmk_is_raise(tw)) {
            json_free(context);
            ret = tw;
            goto cleanup;
        }
        json_free(tw);
        json_t *aca = tmk_args0();
        tmk_aset(aca, "context", context);
        tmk_aset(aca, "params", params);
        tmk_aset(aca, "ref", ref);
        tmk_aset(aca, "owner_db", owner_db);
        tmk_asets(aca, "request_id", request_id);
        json_t *ac = tmk_admit_claim(ctx, aca);
        json_free(context);
        if (tmk_is_raise(ac)) { ret = ac; goto cleanup; }
        json_free(ac);
    }
    /* await connection.resume(ref, {}) */
    {
        const json_t *rsid = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        json_t *rsa = tmk_args0();
        tmk_aset(rsa, "ref", rsid);
        json_set(rsa, "params", json_object());
        json_t *rr = tmk_call(ctx, "connection.resume", rsa);
        if (tmk_is_raise(rr)) { ret = rr; goto cleanup; }
        json_free(rr);
    }
    /* committed = None if saved is None else await to_thread(find, ...) */
    if (saved && saved->type != JSON_NULL) {
        const json_t *actor4 = tmk_attr(connection, "actor", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        const json_t *subject2 = tmk_attr(actor4, "subject", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        const json_t *rsid2 = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { ret = err; goto cleanup; }
        json_t *kw = json_object();
        tmk_asets(kw, "principal_id", tmk_str(subject2));
        tmk_asets(kw, "session_id", tmk_str(rsid2));
        tmk_asets(kw, "request_id", request_id);
        json_t *tta = tmk_args0();
        tmk_asets(tta, "fn", "find");
        json_set(tta, "kwargs", kw);
        committed = tmk_call(ctx, "asyncio.to_thread", tta);
        if (tmk_is_raise(committed)) { ret = committed; committed = NULL; goto cleanup; }
    }
    /* payload / card composition */
    {
        const json_t *ptid = tmk_sub(params, "task_id", &err);
        if (err) { ret = err; goto cleanup; }
        char *ts = tmk_pystr(ptid);
        char *text = malloc(strlen(ts) + 32);
        if (text) sprintf(text, "work kanban task %s", ts);
        free(ts);
        json_t *payload = json_object();
        tmk_asets(payload, "text", text ? text : "");
        free(text);
        const json_t *cpayload = NULL;
        if (committed && committed->type != JSON_NULL) {
            cpayload = tmk_attr(committed, "payload", TMK_NS, &err);
            if (err) { json_free(payload); ret = err; goto cleanup; }
        }
        if (!committed || committed->type == JSON_NULL ||
            (cpayload && json_obj_get(cpayload, "finite")))
            tmk_asetb(payload, "finite", true);
        json_t *card = NULL;
        if (!committed || committed->type == JSON_NULL ||
            (cpayload && json_obj_get(cpayload, "kanban_card_v1"))) {
            /* context.get('board') — re-read the parsed kanban context */
            const json_t *kj3 = tmk_attr(policy, "kanban_json", "Policy", &err);
            if (err) { json_free(payload); ret = err; goto cleanup; }
            json_t *ctx3 = tmk_loads(tmk_str(kj3), &err);
            if (err) { json_free(payload); ret = err; goto cleanup; }
            const json_t *ctid = tmk_sub(params, "task_id", &err);
            if (err) { json_free(payload); json_free(ctx3); ret = err; goto cleanup; }
            const json_t *crid = tmk_sub(params, "run_id", &err);
            if (err) { json_free(payload); json_free(ctx3); ret = err; goto cleanup; }
            card = json_object();
            tmk_aset(card, "board", json_obj_get(ctx3, "board"));
            tmk_aset(card, "task_id", ctid);
            tmk_aset(card, "run_id", crid);
            json_free(ctx3);
        }
        /* authority.submit(actor, Submission(request_id, ref, payload, 'queue'),
         * _kanban_card=card) */
        const json_t *actor5 = tmk_attr(connection, "actor", TMK_NS, &err);
        if (err) { json_free(payload); json_free(card); ret = err; goto cleanup; }
        const json_t *rsid3 = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { json_free(payload); json_free(card); ret = err; goto cleanup; }
        json_t *submission = json_object();
        tmk_asets(submission, "request_id", request_id);
        tmk_aset(submission, "ref", rsid3);
        tmk_aset(submission, "payload", payload);
        tmk_asets(submission, "intent", "queue");
        json_t *sub_a = tmk_args0();
        tmk_aset(sub_a, "actor", actor5);
        tmk_aset(sub_a, "submission", submission);
        tmk_aset(sub_a, "kanban_card", card);
        json_free(card);
        json_t *receipt = tmk_call(ctx, "authority.submit", sub_a);
        if (tmk_is_raise(receipt)) { ret = receipt; goto cleanup; }
        const json_t *rsid4 = tmk_attr(ref, "session_id", TMK_NS, &err);
        if (err) { json_free(receipt); ret = err; goto cleanup; }
        ret = json_object();
        tmk_aset(ret, "session_id", rsid4);
        tmk_aset(ret, "receipt", receipt);
    }
cleanup:
    json_free(committed);
    json_free(saved);
    json_free(policy);
    json_free(ref);
    json_free(store);
    json_free(owner_db);
    free(request_id);
    free(sid);
    return ret ? ret : json_null();
}

/* PoP: tmk_run_task_turns @ gateway/session_kanban.py:_run_task_turns */
static json_t *tmk_run_task_turns(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *frame = json_obj_get(args, "frame");
    const json_t *history = json_obj_get(args, "history");
    const json_t *context = json_obj_get(args, "context");
    const json_t *agent = json_obj_get(args, "agent");
    json_t *err = NULL;
    const json_t *skills_v = tmk_sub(context, "skills", &err);
    if (err) return err;
    const json_t *sid = tmk_attr(agent, "session_id", TMK_NS, &err);
    if (err) return err;
    json_t *ba = tmk_args0();
    tmk_aset(ba, "skills", skills_v);
    tmk_aset(ba, "task_id", sid);
    json_t *built = tmk_call(ctx, "build_preloaded_skills_prompt", ba);
    if (tmk_is_raise(built)) return built;
    const json_t *skills_prompt = json_get(built, 0);
    const json_t *loaded = json_get(built, 1);
    const json_t *missing = json_get(built, 2);
    if (tmk_truthy(missing) && !tmk_truthy(loaded)) {
        json_free(built);
        return tmk_err_spec("ValueError", "kanban_skills_unavailable");
    }
    /* prompt = '\n\n'.join(p for p in (skills, context['context'], frame['text']) if p) */
    const json_t *ccontext = tmk_sub(context, "context", &err);
    if (err) { json_free(built); return err; }
    const json_t *text = tmk_sub(frame, "text", &err);
    if (err) { json_free(built); return err; }
    tmk_sbuf_t pb;
    tmk_sb_init(&pb);
    const json_t *parts[3] = {skills_prompt, ccontext, text};
    bool first = true;
    for (int i = 0; i < 3; i++) {
        if (tmk_truthy(parts[i])) {
            if (!first) tmk_sb_puts(&pb, "\n\n");
            tmk_sb_puts(&pb, tmk_str(parts[i]));
            first = false;
        }
    }
    json_free(built);
    json_t *ra = tmk_args0();
    tmk_asets(ra, "text", pb.buf ? pb.buf : "");
    free(pb.buf);
    tmk_aset(ra, "conversation_history", history);
    json_set(ra, "turn_author", tmk_turn_author());
    json_t *result = tmk_call(ctx, "agent.run_conversation", ra);
    if (tmk_is_raise(result)) return result;
    /* recover_failed_kanban_turns(recover, lambda: result, task_id=,
     * claim_check=lambda: _bound_claim_is_live(context)) — callbacks not driven */
    const json_t *tid = tmk_sub(context, "task_id", &err);
    if (err) { json_free(result); return err; }
    json_t *rfa = tmk_args0();
    tmk_asets(rfa, "turn_fn", "recover");
    tmk_asets(rfa, "get_result", "<lambda>");
    tmk_asets(rfa, "task_id", tmk_str(tid));
    tmk_asets(rfa, "claim_check", "<lambda>");
    json_t *rf = tmk_call(ctx, "recover_failed_kanban_turns", rfa);
    if (tmk_is_raise(rf)) { json_free(result); return rf; }
    json_free(rf);
    /* if not context['goal_mode'] or turn_exit_code(result, ...) != 0: return */
    const json_t *goal_mode = tmk_sub(context, "goal_mode", &err);
    if (err) { json_free(result); return err; }
    if (tmk_truthy(goal_mode)) {
        json_t *ta = tmk_args0();
        tmk_aset(ta, "result", result);
        tmk_asetb(ta, "kanban_worker", true);
        json_t *tc = tmk_call(ctx, "turn_exit_code", ta);
        if (tmk_is_raise(tc)) { json_free(result); return tc; }
        json_t *zero = json_number(0);
        bool settled = tmk_json_eq(tc, zero);   /* != 0 — bool False == 0 */
        json_free(zero);
        json_free(tc);
        if (!settled) return result;
        /* run_kanban_goal_loop(...) */
        const json_t *tid2 = tmk_sub(context, "task_id", &err);
        if (err) { json_free(result); return err; }
        const json_t *goal_text = tmk_sub(context, "goal_text", &err);
        if (err) { json_free(result); return err; }
        const json_t *gmt = tmk_sub(context, "goal_max_turns", &err);
        if (err) { json_free(result); return err; }
        double max_turns = tmk_truthy(gmt) ? gmt->num_val : 20.0;  /* DEFAULT_MAX_TURNS */
        const json_t *fr = json_obj_get(result, "final_response");
        json_t *ga = tmk_args0();
        tmk_asets(ga, "task_id", tmk_str(tid2));
        tmk_asets(ga, "goal_text", tmk_str(goal_text));
        tmk_asets(ga, "run_turn", "turn");
        tmk_asets(ga, "task_status_fn", "status");
        tmk_asets(ga, "block_fn", "block");
        tmk_asetn(ga, "max_turns", max_turns);
        tmk_aset(ga, "first_response", fr ? fr : json_string(""));
        json_t *gl = tmk_call(ctx, "run_kanban_goal_loop", ga);
        if (tmk_is_raise(gl)) { json_free(result); return gl; }
        json_free(gl);
    }
    return result;
}

/* PoP: tmk_recover @ gateway/session_kanban.py:recover */
static json_t *tmk_recover(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *result = json_obj_get(args, "result");
    const json_t *nudge = json_obj_get(args, "nudge");
    const json_t *prior = (result && result->type == JSON_OBJECT)
        ? json_obj_get(result, "messages") : NULL;
    json_t *history = NULL;
    if (tmk_truthy(prior)) {
        history = json_copy(prior);
    } else {
        json_t *sa = tmk_args0();
        tmk_asets(sa, "agent", "agent");
        history = tmk_call(ctx, "stored_conversation", sa);
        if (tmk_is_raise(history)) return history;
    }
    json_t *ra = tmk_args0();
    tmk_asets(ra, "text", tmk_str(nudge));
    tmk_aset(ra, "conversation_history", history);
    json_set(ra, "turn_author", tmk_turn_author());
    json_t *new_result = tmk_call(ctx, "agent.run_conversation", ra);
    if (tmk_is_raise(new_result)) return new_result;
    json_set(ctx->state, "result", json_copy(new_result));
    json_free(new_result);
    return json_null();
}

/* PoP: tmk_status @ gateway/session_kanban.py:status */
static json_t *tmk_status(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    json_t *r = tmk_connect_closing(ctx, NULL);
    if (tmk_is_raise(r)) return r;
    json_free(r);
    json_t *err = NULL;
    const json_t *tid = tmk_sub(context, "task_id", &err);
    if (err) { json_t *cr = tmk_conn_close(ctx); json_free(cr); return err; }
    const json_t *rid = tmk_sub(context, "run_id", &err);
    if (err) { json_t *cr = tmk_conn_close(ctx); json_free(cr); return err; }
    json_t *ga = tmk_args0();
    tmk_asets(ga, "conn", "conn");
    tmk_asets(ga, "task_id", tmk_str(tid));
    tmk_aset(ga, "run_id", rid);
    json_t *ret = tmk_call(ctx, "kb.goal_run_status", ga);
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    return ret ? ret : json_null();
}

/* PoP: tmk_block @ gateway/session_kanban.py:block */
static json_t *tmk_block(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *context = json_obj_get(args, "context");
    const json_t *reason = json_obj_get(args, "reason");
    json_t *r = tmk_connect_closing(ctx, NULL);
    if (tmk_is_raise(r)) return r;
    json_free(r);
    json_t *err = NULL;
    const json_t *tid = tmk_sub(context, "task_id", &err);
    if (err) { json_t *cr = tmk_conn_close(ctx); json_free(cr); return err; }
    const json_t *rid = tmk_sub(context, "run_id", &err);
    if (err) { json_t *cr = tmk_conn_close(ctx); json_free(cr); return err; }
    json_t *ba = tmk_args0();
    tmk_asets(ba, "conn", "conn");
    tmk_asets(ba, "task_id", tmk_str(tid));
    tmk_aset(ba, "reason", reason);
    tmk_aset(ba, "expected_run_id", rid);
    json_t *ret = tmk_call(ctx, "kb.block_task", ba);
    json_t *cr = tmk_conn_close(ctx);
    if (tmk_is_raise(ret)) {
        json_free(cr);
    } else if (tmk_is_raise(cr)) {
        json_free(ret);
        ret = cr;
    } else {
        json_free(cr);
    }
    if (tmk_is_raise(ret)) return ret;
    json_free(ret);
    return json_null();
}

/* PoP: tmk_turn @ gateway/session_kanban.py:turn */
static json_t *tmk_turn(tmk_ctx_t *ctx, const json_t *args) {
    const json_t *prompt = json_obj_get(args, "prompt");
    json_t *sa = tmk_args0();
    tmk_asets(sa, "agent", "agent");
    json_t *history = tmk_call(ctx, "stored_conversation", sa);
    if (tmk_is_raise(history)) return history;
    json_t *ra = tmk_args0();
    tmk_asets(ra, "text", tmk_str(prompt));
    tmk_aset(ra, "conversation_history", history);
    json_set(ra, "turn_author", tmk_turn_author());
    json_t *result = tmk_call(ctx, "agent.run_conversation", ra);
    if (tmk_is_raise(result)) return result;
    json_set(ctx->state, "result", json_copy(result));
    const json_t *fr = json_obj_get(result, "final_response");
    json_t *ret = fr ? json_copy(fr) : json_string("");
    json_free(result);
    return ret;
}

/* PoP: tmk_kanban_run @ gateway/session_controls.py:kanban_run */
static json_t *tmk_kanban_run(tmk_ctx_t *ctx, const json_t *args) {
    json_t *a = tmk_args0();
    tmk_asets(a, "connection", "connection");
    tmk_aset(a, "params", json_obj_get(args, "params"));
    return tmk_call(ctx, "run_task", a);
}