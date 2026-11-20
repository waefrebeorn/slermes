/*
 * gateway_run_flow3.c — gateway/run.py FLOW cores + leaves, third wave
 * (universal-seam convention, twin of gateway_run_flow2 / run_adapters_flow).
 *
 * Ports 16 UNCLAIMED gateway/run.py functions: the Telegram command-mention
 * rewriter, the turn-inactivity watchdog loop, the builtin-adapter lazy
 * import + instantiation pair, the channel-directory / curator / state.db
 * maintenance / deferred-FTS housekeeping flows, the SIGUSR1 restart-handler
 * factory, the migration-command constant leaf, and six GatewayRunner
 * methods (running-agent id set, bot-message
 * admission under the authorization profile, the authorization-profile scope
 * wrapper, the session-env context scope, the housekeeping executor offload
 * and the housekeeping pool getter). Every live I/O (logging, importlib,
 * threading events, monotonic clock, activity meters, config loaders,
 * persistence handles, scheduling, claimed siblings) enters through
 * gr3_seams_t.call(ctx, name, args); args is owned by gr3_call (freed after
 * the borrowed call); results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every `except Exception` site. Fail-closed: the harness delivers a
 * RuntimeError raise spec for an unscripted seam.
 *
 * Seam name table (name -> live call):
 *   sanitize_telegram_name        hermes_cli.commands_platforms._sanitize_telegram_name(group1)
 *   worker_done.wait              worker_done.wait(max(0.01, poll_interval))
 *   time.monotonic                time.monotonic()
 *   awake_idle.measure            AwakeIdleMeter().measure(seconds)
 *   get_activity_summary          agent.get_activity_summary()
 *   _abandon_timed_out_gateway_turn  claimed sibling (kwargs)
 *   importlib.import_module       importlib.import_module("gateway.platforms."+module)
 *   <AdapterCls>                  adapter_cls(config)  (cls: markers)
 *   <check_fn>                    requirements_ok()    (fn: markers)
 *   logger.warning/info           module logger calls ({fmt, args?})
 *   build_channel_directory       gateway.channel_directory.build_channel_directory(adapters)
 *   safe_schedule_threadsafe      agent.async_utils.safe_schedule_threadsafe(coro, loop, logger=, log_message=)
 *   fut.result                    fut.result(timeout=30)
 *   run_idle_jobs                 hermes_cli.plugins_idle_tasks.run_idle_jobs(idle_for_seconds=, on_summary=, fence=)
 *   load_config                   hermes_cli.config.load_config()
 *   profile_scope                 persistence_assembly.profile_scope()
 *   open_persistence              persistence_assembly.open_persistence(scope, eager=True)
 *   open_store_admin              persistence_assembly.open_store_admin(scope, read_only=False)
 *   native_inputs_root            gateway.session_ingress_media.native_inputs_root()
 *   run_auto_archive              session_retention.run_auto_archive(store, idle_days=, min_interval_hours=)
 *   run_auto_prune_and_vacuum     session_retention.run_auto_prune_and_vacuum(...)
 *   _profile_sessions_dir         claimed sibling _profile_sessions_dir(launch)
 *   store.close                   store.close()
 *   live_stores                   persistence_assembly.live_stores() (CM enter; exit silent)
 *   retry_deferred_search_recovery  store.search.retry_deferred_search_recovery()
 *   store_location                persistence_assembly.store_location(store.scope)
 *   request_restart               runner.request_restart(detached=False, via_service=True)
 *   _running_agent_items          claimed sibling GatewayRunner._running_agent_items()
 *   _authorization_home_for_source  authz-mixin collaborator (source)
 *   _admit_bot_message            authz-mixin collaborator (source)
 *   _profile_runtime_scope.enter/exit  claimed sibling CM (home) / silent reset
 *   _set_session_env / _clear_session_env  claimed siblings (context) / (tokens)
 *   body                          the block driven inside _session_env_scope
 *   check                         the callback driven inside _under_authorization_profile
 *   asyncio.get_running_loop      asyncio.get_running_loop()
 *   _get_or_create_pool           claimed sibling GatewayRunner._get_or_create_pool(self, attr, make_pool)
 *   ThreadPoolExecutor            concurrent.futures.ThreadPoolExecutor(max_workers=4, thread_name_prefix="hermes-gateway-hk")
 *   copy_context                  contextvars.copy_context()
 *   loop.run_in_executor          loop.run_in_executor(executor, ctx.run, func, *args)
 *   MIGRATE_COMMAND               from hermes_cli.gateway_migrate import MIGRATE_COMMAND
 *
 * Modeling notes (differential oracle tests/oracle_gateway_run_flow3.py):
 *  - Callable markers: a JSON string "cls:<Name>" / "fn:<Name>" models a
 *    callable; calling it fires seam <Name>. "cls:" calls carry {config},
 *    "fn:" calls carry {}. Non-marker values are not callable.
 *  - threading.Event/Lock, futures, stores, executors, contexts and agents
 *    travel as marker strings; the oracle keeps attribute doubles whose
 *    methods fire the matching seams and denorm() projects back to markers.
 *  - _watch_gateway_turn_inactivity: agent_holder is a list (truthiness =
 *    non-empty); agent dicts carry a "get_activity_summary" key when the
 *    method exists (hasattr). reported_idle domain: int/float/bool/null;
 *    other types raise TypeError inside the swallowed except. The
 *    AwakeIdleMeter constructor is silent; measure is the seam.
 *  - _telegramize_command_mentions: the regex
 *    (?<![\w:/])/([A-Za-z0-9][A-Za-z0-9_-]*) is transcribed as a left-to-
 *    right scanner over the ORIGINAL text (lookbehind reads the original
 *    preceding byte; ASCII \w domain). import_fail models the in-function
 *    import halting (sys.modules None -> ModuleNotFoundError, empty trace).
 *  - None-returning live flows (watchdog abandon-and-return, channel
 *    directory fut.result, restart closure, session-env CM block) DISCARD
 *    the last seam's non-raise result: the port checks it for a raise spec
 *    then returns NULL. Fixtures script non-null results to prove it.
 *  - _instantiate_builtin_adapter: Platform enums travel as value strings;
 *    the harness/oracle validate membership before dispatch (ValueError
 *    otherwise). _BUILTIN_ADAPTERS is the live 3-entry table. Ported
 *    sibling _builtin_adapter_import composes inline.
 *  - _housekeeping_curator: float("inf") projects to the string "inf" in
 *    seam args (JSON cannot carry inf); on_summary projects to the marker
 *    "on_summary" and the stub invokes it once per in.summary_msgs entry
 *    AFTER run_idle_jobs returns (logger.info "curator: %s").
 *  - _housekeeping_state_db_maintenance: float()/int() coercions accept
 *    int/float/bool (int truncates toward zero); null raises the exact
 *    TypeError; str values are outside the fixture domain. scope is a
 *    SimpleNamespace double (missing profile_dir -> the types.Simple-
 *    Namespace AttributeError). try/finally: store.close fires on every
 *    path after open_persistence succeeds; a close raise replaces the ret.
 *  - _housekeeping_deferred_fts_retry: live_stores is a CM whose enter
 *    fires the seam and whose exit is silent; store dicts carry {marker,
 *    scope}. No except sites: every raise propagates.
 *  - _start_gateway_make_restart_signal_handler: the drive invokes the
 *    returned closure; the C port models construction+invocation in one
 *    call (construction itself fires no seam).
 *  - _running_agent_ids: id(a) projects to the agent marker string; the
 *    pending sentinel travels as {"__sentinel__": true}; the result set
 *    normalizes to a sorted marker list on both sides.
 *  - _session_env_scope / _under_authorization_profile: context-manager
 *    flows driven as enter -> body/check -> finally-exit; an exit raise
 *    replaces the body result/raise (Python finally semantics); an enter
 *    raise skips body and exit.
 *  - _run_housekeeping_in_executor: await is modeled as identity on the
 *    run_in_executor seam result; _get_or_create_pool returning null means
 *    "pool absent -> make_pool()" and fires the ThreadPoolExecutor seam.
 *    copy_context().run projects to the constant "copy_context.run".
 *  - Runner instance methods are presence-gated on the runner dict keys
 *    (missing -> 'GatewayRunner' object has no attribute '<name>');
 *    _get_or_create_pool is class-level and always available.
 *  - str() positions accept the scalar domain (str/int/bool/null);
 *    integral floats serialize int-like and normalize on both sides.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "libjson/json.h"

typedef struct gr3_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gr3_seams_t;

/* ── universal-seam helpers ── */
static json_t *gr3_call(const gr3_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool gr3_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *gr3_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING && k->str_val) ? k->str_val : "";
}
static json_t *gr3_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool gr3_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static bool gr3_truth(const json_t *v) {
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
static const json_t *gr3_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *gr3_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *gr3_args(void) { return json_object(); }
static void gr3_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gr3_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gr3_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gr3_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
/* Python type name for a JSON node (integral numbers read as int). */
static const char *gr3_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* str(v) for the scalar domain; containers are OUT OF DOMAIN. malloc'd. */
static char *gr3_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (isfinite(d) && d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}
/* ── logging seam (RecLogger twin): {fmt, args?} ── */
static json_t *gr3_log(const gr3_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    json_t *a = gr3_args();
    gr3_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    return gr3_call(s, name, a);
}
static json_t *gr3_log1(const gr3_seams_t *s, const char *level,
                        const char *fmt, const json_t *x) {
    json_t *a = json_array();
    json_append(a, x ? json_copy(x) : json_null());
    return gr3_log(s, level, fmt, a);
}
/* ── float()/int() coercion (numeric domain; null -> exact TypeError) ── */
static json_t *gr3_float_coerce(const json_t *v, double *out) {
    if (v && v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    char m[160];
    snprintf(m, sizeof(m),
             "float() argument must be a string or a real number, not '%s'",
             gr3_type_name(v));
    return gr3_raise("TypeError", m);
}
static json_t *gr3_int_coerce(const json_t *v, double *out) {
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        *out = (isfinite(d) && d < 9.2e18 && d > -9.2e18) ? (double)(long long)d : d;
        return NULL;
    }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    char m[160];
    snprintf(m, sizeof(m),
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             gr3_type_name(v));
    return gr3_raise("TypeError", m);
}
/* ── callable markers ("cls:Name" / "fn:Name") ── */
static bool gr3_callable(const json_t *v) {
    const char *s = gr3_sval(v);
    return s && (strncmp(s, "cls:", 4) == 0 || strncmp(s, "fn:", 3) == 0);
}
static const char *gr3_marker_name(const json_t *v) {
    const char *s = gr3_sval(v);
    if (!s) return "";
    if (strncmp(s, "cls:", 4) == 0) return s + 4;
    if (strncmp(s, "fn:", 3) == 0) return s + 3;
    return s;
}
/* ── runner instance-method presence gate + seam ── */
static json_t *gr3_runner_method(const json_t *runner, const char *name,
                                 json_t *args, const gr3_seams_t *s) {
    bool present = runner && runner->type == JSON_OBJECT && json_obj_get(runner, name);
    if (!present) {
        json_free(args);
        const char *tn = (!runner || runner->type == JSON_NULL) ? "NoneType"
                       : (runner->type == JSON_OBJECT) ? "GatewayRunner"
                       : gr3_type_name(runner);
        char m[192];
        snprintf(m, sizeof(m), "'%s' object has no attribute '%s'", tn, name);
        return gr3_raise("AttributeError", m);
    }
    return gr3_call(s, name, args);
}

/* PoP: gr3_telegramize_command_mentions @ gateway/run.py:_telegramize_command_mentions */
/* platform: value string or {"value": ...} double; import_fail models the
 * halted in-function import. Regex (?<![\w:/])/([A-Za-z0-9][A-Za-z0-9_-]*)
 * transcribed as a scanner over the original text (ASCII \w domain). */
static bool gr3_is_word_byte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}
static bool gr3_is_cmd_start(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
static bool gr3_is_cmd_cont(unsigned char c) {
    return gr3_is_cmd_start(c) || c == '_' || c == '-';
}
json_t *gr3_telegramize_command_mentions(const json_t *platform, const char *text,
                                         bool import_fail, const gr3_seams_t *s) {
    if (!text) text = "";
    /* getattr(platform, "value", platform) */
    const char *pv = NULL;
    if (platform && platform->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(platform, "value");
        pv = gr3_sval(v);
        if (!v) pv = NULL; /* attribute absent -> default is the double itself */
    } else {
        pv = gr3_sval(platform);
    }
    bool is_tg = pv && strcmp(pv, "telegram") == 0;
    if (!is_tg) return json_string(text);
    if (import_fail)
        return gr3_raise("ModuleNotFoundError",
            "import of hermes_cli.commands_platforms halted; None in sys.modules");
    size_t n = strlen(text);
    size_t cap = n + 64;
    char *out = malloc(cap);
    size_t len = 0;
    if (!out) return gr3_raise("MemoryError", "out of memory");
    size_t i = 0;
    while (i < n) {
        bool match = false;
        size_t run_end = 0;
        if (text[i] == '/' && (i == 0 || !(gr3_is_word_byte((unsigned char)text[i-1])
                                           || text[i-1] == ':' || text[i-1] == '/'))
            && i + 1 < n && gr3_is_cmd_start((unsigned char)text[i+1])) {
            run_end = i + 2;
            while (run_end < n && gr3_is_cmd_cont((unsigned char)text[run_end])) run_end++;
            match = true;
        }
        if (!match) {
            if (len + 1 >= cap) { cap *= 2; char *t = realloc(out, cap); if (!t) { free(out); return gr3_raise("MemoryError", "out of memory"); } out = t; }
            out[len++] = text[i++];
            continue;
        }
        size_t glen = run_end - (i + 1);
        char *group = malloc(glen + 1);
        if (!group) { free(out); return gr3_raise("MemoryError", "out of memory"); }
        memcpy(group, text + i + 1, glen);
        group[glen] = '\0';
        json_t *a = gr3_args();
        gr3_s(a, "name", group);
        json_t *r = gr3_call(s, "sanitize_telegram_name", a);
        free(group);
        if (gr3_raised(r)) { free(out); return r; }
        /* f"/{sanitized}" if sanitized else match.group(0) */
        const char *repl = NULL;
        char *repl_buf = NULL;
        if (gr3_truth(r)) {
            repl_buf = gr3_pystr(r);
            repl = repl_buf;
        }
        size_t add = repl ? strlen(repl) + 1 : run_end - i;
        if (len + add >= cap) {
            while (len + add >= cap) cap *= 2;
            char *t = realloc(out, cap);
            if (!t) { free(out); free(repl_buf); json_free(r); return gr3_raise("MemoryError", "out of memory"); }
            out = t;
        }
        if (repl) {
            out[len++] = '/';
            memcpy(out + len, repl, strlen(repl));
            len += strlen(repl);
        } else {
            memcpy(out + len, text + i, run_end - i);
            len += run_end - i;
        }
        free(repl_buf);
        json_free(r);
        i = run_end;
    }
    out[len] = '\0';
    json_t *ret = json_string(out);
    free(out);
    return ret;
}

/* PoP: gr3_watch_gateway_turn_inactivity @ gateway/run.py:_watch_gateway_turn_inactivity */
json_t *gr3_watch_gateway_turn_inactivity(const json_t *agent_holder,
        const char *task_id, const json_t *process_baseline, double timeout,
        const char *worker_done, const char *timeout_fired, const char *cleanup_lock,
        double poll_interval, const char *is_still_current, const gr3_seams_t *s) {
    json_t *or_ = gr3_call(s, "time.monotonic", gr3_args());
    if (gr3_raised(or_)) return or_;
    double activity_origin = (or_ && or_->type == JSON_NUMBER) ? or_->num_val : 0.0;
    json_free(or_);
    /* AwakeIdleMeter() construction is silent; measure is the seam. */
    for (;;) {
        double wait_arg = poll_interval > 0.01 ? poll_interval : 0.01;
        json_t *wa = gr3_args();
        gr3_n(wa, "timeout", wait_arg);
        json_t *wr = gr3_call(s, "worker_done.wait", wa);
        if (gr3_raised(wr)) return wr;
        bool done = gr3_truth(wr);
        json_free(wr);
        if (done) return NULL;
        json_t *nr = gr3_call(s, "time.monotonic", gr3_args());
        if (gr3_raised(nr)) return nr;
        double now = (nr && nr->type == JSON_NUMBER) ? nr->num_val : 0.0;
        json_free(nr);
        double idle_seconds = now - activity_origin;
        /* agent = agent_holder[0] if agent_holder else None */
        const json_t *agent = NULL;
        if (agent_holder && agent_holder->type == JSON_ARRAY && json_len(agent_holder) > 0)
            agent = json_get(agent_holder, 0);
        if (agent && agent->type != JSON_NULL && gr3_oget(agent, "get_activity_summary")) {
            /* try: activity read; except Exception: pass */
            json_t *sr = gr3_call(s, "get_activity_summary", gr3_args());
            if (gr3_raised(sr)) {
                if (gr3_is_base_exc(gr3_kind(sr))) return sr;
                json_free(sr);
            } else if (!sr || sr->type != JSON_OBJECT) {
                /* .get on a non-dict raises AttributeError (swallowed) */
                json_free(sr);
            } else {
                const json_t *rep = json_obj_get(sr, "seconds_since_activity");
                if (rep && rep->type != JSON_NULL) {
                    double fv = 0.0;
                    json_t *err = gr3_float_coerce(rep, &fv);
                    if (err) {
                        /* TypeError swallowed by except Exception */
                        json_free(err);
                    } else {
                        double mx = fv > 0.0 ? fv : 0.0;
                        json_t *ma = gr3_args();
                        gr3_n(ma, "seconds", mx);
                        json_t *mr = gr3_call(s, "awake_idle.measure", ma);
                        if (gr3_raised(mr)) {
                            if (gr3_is_base_exc(gr3_kind(mr))) { json_free(sr); return mr; }
                            json_free(mr);
                        } else {
                            idle_seconds = (mr && mr->type == JSON_NUMBER) ? mr->num_val : 0.0;
                            json_free(mr);
                            activity_origin = now - idle_seconds;
                        }
                    }
                }
                json_free(sr);
            }
        }
        if (idle_seconds < timeout) continue;
        json_t *aa = gr3_args();
        gr3_j(aa, "agent_holder", agent_holder);
        gr3_s(aa, "task_id", task_id);
        gr3_j(aa, "process_baseline", process_baseline);
        gr3_s(aa, "worker_done", worker_done);
        gr3_s(aa, "timeout_fired", timeout_fired);
        gr3_s(aa, "cleanup_lock", cleanup_lock);
        gr3_s(aa, "is_still_current", is_still_current);
        json_t *ar = gr3_call(s, "_abandon_timed_out_gateway_turn", aa);
        if (gr3_raised(ar)) return ar;
        json_free(ar);
        return NULL; /* live: _abandon(...); return -> None */
    }
}

/* PoP: gr3_builtin_adapter_import @ gateway/run.py:_builtin_adapter_import */
json_t *gr3_builtin_adapter_import(const char *module, const char *adapter_name,
                                   const char *requirement, const gr3_seams_t *s) {
    char full[256];
    snprintf(full, sizeof(full), "gateway.platforms.%s", module ? module : "");
    json_t *a = gr3_args();
    gr3_s(a, "module", full);
    json_t *mod = gr3_call(s, "importlib.import_module", a);
    if (gr3_raised(mod)) return mod;
    const char *names[2] = { adapter_name, requirement };
    json_t *vals[2] = { NULL, NULL };
    for (int k = 0; k < 2; k++) {
        const json_t *v = gr3_oget(mod, names[k]);
        if (!v) {
            char m[320];
            if (mod && mod->type == JSON_OBJECT)
                snprintf(m, sizeof(m), "module '%s' has no attribute '%s'", full, names[k]);
            else
                snprintf(m, sizeof(m), "'%s' object has no attribute '%s'",
                         gr3_type_name(mod), names[k]);
            json_free(mod);
            json_free(vals[0]);
            return gr3_raise("AttributeError", m);
        }
        vals[k] = json_copy(v);
    }
    json_free(mod);
    json_t *ret = json_array();
    json_append(ret, vals[0]);
    json_append(ret, vals[1]);
    return ret;
}

/* _BUILTIN_ADAPTERS (live 3-entry table, Platform value strings as keys). */
typedef struct {
    const char *platform;
    const char *module;
    const char *adapter_name;
    const char *requirement;
    const char *warning;
} gr3_builtin_spec_t;
static const gr3_builtin_spec_t GR3_BUILTIN_ADAPTERS[] = {
    {"api_server", "api_server", "APIServerAdapter", "check_api_server_requirements",
     "API Server: aiohttp not installed"},
    {"webhook", "webhook", "WebhookAdapter", "check_webhook_requirements",
     "Webhook: aiohttp not installed"},
    {"msgraph_webhook", "msgraph_webhook", "MSGraphWebhookAdapter",
     "check_msgraph_webhook_requirements", "MSGraph webhook: aiohttp not installed"},
};

/* PoP: gr3_instantiate_builtin_adapter @ gateway/run.py:_instantiate_builtin_adapter */
json_t *gr3_instantiate_builtin_adapter(const char *platform_value,
                                        const json_t *config, const gr3_seams_t *s) {
    const gr3_builtin_spec_t *spec = NULL;
    for (size_t i = 0; i < sizeof(GR3_BUILTIN_ADAPTERS)/sizeof(GR3_BUILTIN_ADAPTERS[0]); i++) {
        if (platform_value && strcmp(platform_value, GR3_BUILTIN_ADAPTERS[i].platform) == 0) {
            spec = &GR3_BUILTIN_ADAPTERS[i];
            break;
        }
    }
    if (!spec) return NULL;
    /* ported sibling composes inline */
    json_t *pair = gr3_builtin_adapter_import(spec->module, spec->adapter_name,
                                              spec->requirement, s);
    if (gr3_raised(pair)) return pair;
    json_t *adapter_cls = json_copy(json_get(pair, 0));
    json_t *requirements_ok = json_copy(json_get(pair, 1));
    json_free(pair);
    /* requirements_ok() if callable(requirements_ok) else requirements_ok */
    bool ok;
    if (gr3_callable(requirements_ok)) {
        json_t *r = gr3_call(s, gr3_marker_name(requirements_ok), gr3_args());
        if (gr3_raised(r)) { json_free(adapter_cls); json_free(requirements_ok); return r; }
        ok = gr3_truth(r);
        json_free(r);
    } else {
        ok = gr3_truth(requirements_ok);
    }
    json_free(requirements_ok);
    if (!ok) {
        json_free(adapter_cls);
        json_t *r = gr3_log(s, "warning", spec->warning, NULL);
        if (gr3_raised(r)) return r;
        json_free(r);
        return NULL;
    }
    if (!gr3_callable(adapter_cls)) {
        char m[160];
        snprintf(m, sizeof(m), "'%s' object is not callable", gr3_type_name(adapter_cls));
        json_free(adapter_cls);
        return gr3_raise("TypeError", m);
    }
    json_t *a = gr3_args();
    gr3_j(a, "config", config);
    json_t *r = gr3_call(s, gr3_marker_name(adapter_cls), a);
    json_free(adapter_cls);
    return r;
}

/* PoP: gr3_housekeeping_channel_directory @ gateway/run.py:_housekeeping_channel_directory */
json_t *gr3_housekeeping_channel_directory(const json_t *adapters, const char *loop,
                                           const gr3_seams_t *s) {
    if (!loop) return NULL;
    json_t *ba = gr3_args();
    gr3_j(ba, "adapters", adapters);
    json_t *coro = gr3_call(s, "build_channel_directory", ba);
    if (gr3_raised(coro)) return coro;
    json_t *sa = gr3_args();
    gr3_j(sa, "coro", coro);
    gr3_s(sa, "loop", loop);
    gr3_s(sa, "logger", "logger");
    gr3_s(sa, "log_message", "Channel directory refresh scheduling error");
    json_free(coro);
    json_t *fut = gr3_call(s, "safe_schedule_threadsafe", sa);
    if (gr3_raised(fut)) return fut;
    if (!fut || fut->type == JSON_NULL) { json_free(fut); return NULL; }
    json_free(fut);
    json_t *fa = gr3_args();
    gr3_n(fa, "timeout", 30);
    json_t *fr = gr3_call(s, "fut.result", fa);
    if (gr3_raised(fr)) return fr;
    json_free(fr);
    return NULL; /* live: fut.result(timeout=30) then implicit None */
}

/* PoP: gr3_housekeeping_curator @ gateway/run.py:_housekeeping_curator */
/* float("inf") projects to "inf"; on_summary fires logger.info per
 * in.summary_msgs entry AFTER run_idle_jobs returns (stub convention). */
json_t *gr3_housekeeping_curator(const json_t *idle_fence, const json_t *summary_msgs,
                                 const gr3_seams_t *s) {
    json_t *a = gr3_args();
    gr3_s(a, "idle_for_seconds", "inf");
    gr3_s(a, "on_summary", "on_summary");
    gr3_j(a, "fence", idle_fence);
    json_t *r = gr3_call(s, "run_idle_jobs", a);
    if (gr3_raised(r)) return r;
    json_free(r);
    if (summary_msgs && summary_msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(summary_msgs); i++) {
            json_t *lr = gr3_log1(s, "info", "curator: %s", json_get(summary_msgs, i));
            if (gr3_raised(lr)) return lr;
            json_free(lr);
        }
    }
    return NULL;
}

/* PoP: gr3_housekeeping_state_db_maintenance @ gateway/run.py:_housekeeping_state_db_maintenance */
json_t *gr3_housekeeping_state_db_maintenance(const json_t *launch, const gr3_seams_t *s) {
    json_t *cfg = gr3_call(s, "load_config", gr3_args());
    if (gr3_raised(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        char m[160];
        snprintf(m, sizeof(m), "'%s' object has no attribute 'get'", gr3_type_name(cfg));
        json_free(cfg);
        return gr3_raise("AttributeError", m);
    }
    const json_t *sess = json_obj_get(cfg, "sessions");
    if (!gr3_truth(sess)) sess = NULL; /* `or {}` */
    bool auto_archive = gr3_truth(gr3_oget(sess, "auto_archive"));
    bool auto_prune = gr3_truth(gr3_oget(sess, "auto_prune"));
    if (!auto_archive && !auto_prune) { json_free(cfg); return NULL; }

    json_t *scope = gr3_call(s, "profile_scope", gr3_args());
    if (gr3_raised(scope)) { json_free(cfg); return scope; }
    json_t *pa = gr3_args();
    gr3_j(pa, "scope", scope);
    gr3_b(pa, "eager", true);
    json_t *store = gr3_call(s, "open_persistence", pa);
    if (gr3_raised(store)) { json_free(scope); json_free(cfg); return store; }

    json_t *ret = NULL;
    /* try: */
    if (auto_archive) {
        const json_t *dv = gr3_oget(sess, "auto_archive_days");
        json_t *dflt = NULL;
        if (!dv) { dflt = json_number(3); dv = dflt; }
        double idle_days = 0;
        ret = gr3_float_coerce(dv, &idle_days);
        json_free(dflt);
        if (ret) goto finally;
        const json_t *mv = gr3_oget(sess, "min_interval_hours");
        json_t *mdef = NULL;
        if (!mv) { mdef = json_number(24); mv = mdef; }
        double min_int = 0;
        ret = gr3_int_coerce(mv, &min_int);
        json_free(mdef);
        if (ret) goto finally;
        json_t *aa = gr3_args();
        gr3_j(aa, "store", store);
        gr3_n(aa, "idle_days", idle_days);
        gr3_n(aa, "min_interval_hours", min_int);
        ret = gr3_call(s, "run_auto_archive", aa);
        if (gr3_raised(ret)) goto finally;
        json_free(ret); ret = NULL;
    }
    if (auto_prune) {
        json_t *oa = gr3_args();
        gr3_j(oa, "scope", scope);
        gr3_b(oa, "read_only", false);
        json_t *admin = gr3_call(s, "open_store_admin", oa);
        if (gr3_raised(admin)) { ret = admin; goto finally; }
        /* store_home = scope.profile_dir (direct attribute read) */
        const json_t *sh = gr3_oget(scope, "profile_dir");
        if (!sh) {
            char m[192];
            snprintf(m, sizeof(m), "'%s' object has no attribute 'profile_dir'",
                     (scope && scope->type == JSON_OBJECT) ? "types.SimpleNamespace"
                     : gr3_type_name(scope));
            json_free(admin);
            ret = gr3_raise("AttributeError", m);
            goto finally;
        }
        json_t *nr = gr3_call(s, "native_inputs_root", gr3_args());
        if (gr3_raised(nr)) { json_free(admin); ret = nr; goto finally; }
        char *media_root = gr3_pystr(nr);
        json_free(nr);
        double retention = 0, min_int = 0, min_vac = 0;
        const json_t *rv = gr3_oget(sess, "retention_days");
        json_t *rdef = NULL;
        if (!rv) { rdef = json_number(90); rv = rdef; }
        ret = gr3_int_coerce(rv, &retention);
        json_free(rdef);
        if (ret) { free(media_root); json_free(admin); goto finally; }
        const json_t *mv = gr3_oget(sess, "min_interval_hours");
        json_t *mdef = NULL;
        if (!mv) { mdef = json_number(24); mv = mdef; }
        ret = gr3_int_coerce(mv, &min_int);
        json_free(mdef);
        if (ret) { free(media_root); json_free(admin); goto finally; }
        const json_t *vv = gr3_oget(sess, "min_vacuum_interval_days");
        json_t *vdef = NULL;
        if (!vv) { vdef = json_number(30); vv = vdef; }
        ret = gr3_int_coerce(vv, &min_vac);
        json_free(vdef);
        if (ret) { free(media_root); json_free(admin); goto finally; }
        const json_t *vac = gr3_oget(sess, "vacuum_after_prune");
        bool vacuum = vac ? gr3_truth(vac) : true;
        json_t *la = gr3_args();
        gr3_j(la, "launch", launch);
        json_t *sessions_dir = gr3_call(s, "_profile_sessions_dir", la);
        if (gr3_raised(sessions_dir)) {
            free(media_root); json_free(admin);
            ret = sessions_dir; goto finally;
        }
        json_t *ra = gr3_args();
        gr3_j(ra, "store", store);
        gr3_j(ra, "admin", admin);
        gr3_s(ra, "store_home", gr3_sval(sh));
        gr3_s(ra, "media_root", media_root);
        gr3_n(ra, "retention_days", retention);
        gr3_n(ra, "min_interval_hours", min_int);
        gr3_n(ra, "min_vacuum_interval_days", min_vac);
        gr3_b(ra, "vacuum", vacuum);
        gr3_j(ra, "sessions_dir", sessions_dir);
        json_free(admin);
        free(media_root);
        json_free(sessions_dir);
        ret = gr3_call(s, "run_auto_prune_and_vacuum", ra);
        if (gr3_raised(ret)) goto finally;
        json_free(ret); ret = NULL;
    }
finally:
    {
        json_t *cr = gr3_call(s, "store.close", gr3_args());
        if (gr3_raised(cr)) { json_free(ret); ret = cr; }
        else json_free(cr);
    }
    json_free(scope);
    json_free(store);
    json_free(cfg);
    return ret;
}

/* PoP: gr3_housekeeping_deferred_fts_retry @ gateway/run.py:_housekeeping_deferred_fts_retry */
json_t *gr3_housekeeping_deferred_fts_retry(const gr3_seams_t *s) {
    json_t *stores = gr3_call(s, "live_stores", gr3_args());
    if (gr3_raised(stores)) return stores;
    if (!stores || stores->type != JSON_ARRAY) {
        char m[160];
        snprintf(m, sizeof(m), "'%s' object is not iterable", gr3_type_name(stores));
        json_free(stores);
        return gr3_raise("TypeError", m);
    }
    for (size_t i = 0; i < json_len(stores); i++) {
        const json_t *st = json_get(stores, i);
        const char *marker = json_get_str(st, "marker", "");
        json_t *ra = gr3_args();
        gr3_s(ra, "store", marker);
        json_t *r = gr3_call(s, "retry_deferred_search_recovery", ra);
        if (gr3_raised(r)) { json_free(stores); return r; }
        bool recovered = gr3_truth(r);
        json_free(r);
        if (!recovered) continue;
        json_t *la = gr3_args();
        gr3_j(la, "scope", gr3_oget(st, "scope"));
        json_t *loc = gr3_call(s, "store_location", la);
        if (gr3_raised(loc)) { json_free(stores); return loc; }
        json_t *lr = gr3_log1(s, "info",
            "Deferred state.db FTS rebuild completed in-process for %s; full-text search restored.",
            loc);
        json_free(loc);
        if (gr3_raised(lr)) { json_free(stores); return lr; }
        json_free(lr);
    }
    json_free(stores);
    return NULL;
}

/* PoP: gr3_start_gateway_make_restart_signal_handler @ gateway/run.py:_start_gateway_make_restart_signal_handler */
/* Models construction + invocation of the returned closure (the drive
 * calls handler() immediately; construction fires no seam). */
json_t *gr3_start_gateway_make_restart_signal_handler(const json_t *runner,
                                                      const gr3_seams_t *s) {
    json_t *lr = gr3_log(s, "info",
        "SIGUSR1 received (systemctl reload / hermes gateway restart): performing a graceful "
        "gateway restart \xe2\x80\x94 drain active turns, exit, supervisor relaunches. Not an in-process "
        "config reload.", NULL);
    if (gr3_raised(lr)) return lr;
    json_free(lr);
    json_t *a = gr3_args();
    gr3_b(a, "detached", false);
    gr3_b(a, "via_service", true);
    json_t *rr = gr3_runner_method(runner, "request_restart", a, s);
    if (gr3_raised(rr)) return rr;
    json_free(rr);
    return NULL; /* the closure body ends with the call -> None */
}

/* PoP: gr3_running_agent_ids @ gateway/run.py:GatewayRunner._running_agent_ids */
/* id(a) projects to the agent marker; sentinel = {"__sentinel__": true};
 * the result set normalizes to a sorted marker list. */
static int gr3_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
json_t *gr3_running_agent_ids(const json_t *runner, const gr3_seams_t *s) {
    json_t *items = gr3_runner_method(runner, "_running_agent_items", gr3_args(), s);
    if (gr3_raised(items)) return items;
    if (!items || items->type != JSON_ARRAY) {
        char m[160];
        snprintf(m, sizeof(m), "'%s' object is not iterable", gr3_type_name(items));
        json_free(items);
        return gr3_raise("TypeError", m);
    }
    const char **ids = NULL;
    size_t nids = 0, cap = 0;
    for (size_t i = 0; i < json_len(items); i++) {
        const json_t *pair = json_get(items, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) continue; /* domain: pairs */
        const json_t *agent = json_get(pair, 1);
        if (!agent || agent->type == JSON_NULL) continue;
        if (agent->type == JSON_OBJECT && json_obj_get(agent, "__sentinel__")) continue;
        const char *marker = gr3_sval(agent);
        if (!marker) continue; /* domain: marker strings */
        bool dup = false;
        for (size_t k = 0; k < nids; k++) if (strcmp(ids[k], marker) == 0) { dup = true; break; }
        if (dup) continue;
        if (nids == cap) {
            cap = cap ? cap * 2 : 8;
            const char **t = realloc(ids, cap * sizeof(*ids));
            if (!t) { free(ids); json_free(items); return gr3_raise("MemoryError", "out of memory"); }
            ids = t;
        }
        ids[nids++] = strdup(marker);
    }
    json_free(items);
    if (nids > 1) qsort(ids, nids, sizeof(*ids), gr3_str_cmp);
    json_t *ret = json_array();
    for (size_t k = 0; k < nids; k++) { json_append(ret, json_string(ids[k])); free((void *)ids[k]); }
    free(ids);
    return ret;
}

/* shared authorization-profile scope flow: check_ev is the seam fired for
 * the check callback ("_admit_bot_message" carries {source}, "check" {}). */
static json_t *gr3_under_auth_profile(const json_t *runner, const json_t *source,
                                      const char *check_ev, const gr3_seams_t *s) {
    json_t *ha = gr3_args();
    gr3_j(ha, "source", source);
    json_t *home = gr3_runner_method(runner, "_authorization_home_for_source", ha, s);
    if (gr3_raised(home)) return home;
    json_t *check_r = NULL;
    if (!home || home->type == JSON_NULL) {
        json_free(home);
        if (strcmp(check_ev, "check") == 0) {
            check_r = gr3_call(s, "check", gr3_args());
        } else {
            json_t *ca = gr3_args();
            gr3_j(ca, "source", source);
            check_r = gr3_runner_method(runner, check_ev, ca, s);
        }
        return check_r;
    }
    const char *hs = gr3_sval(home);
    if (!hs) {
        char m[192];
        snprintf(m, sizeof(m),
                 "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
                 gr3_type_name(home));
        json_free(home);
        return gr3_raise("TypeError", m);
    }
    json_t *ea = gr3_args();
    gr3_s(ea, "home", hs);
    json_t *er = gr3_call(s, "_profile_runtime_scope.enter", ea);
    if (gr3_raised(er)) { json_free(home); return er; }
    json_free(er);
    if (strcmp(check_ev, "check") == 0) {
        check_r = gr3_call(s, "check", gr3_args());
    } else {
        json_t *ca = gr3_args();
        gr3_j(ca, "source", source);
        check_r = gr3_runner_method(runner, check_ev, ca, s);
    }
    json_t *xr = gr3_call(s, "_profile_runtime_scope.exit", gr3_args());
    json_free(home);
    if (gr3_raised(xr)) { json_free(check_r); return xr; }
    json_free(xr);
    return check_r;
}

/* PoP: gr3_admit_bot_message_for_source @ gateway/run.py:GatewayRunner._admit_bot_message_for_source */
json_t *gr3_admit_bot_message_for_source(const json_t *runner, const json_t *source,
                                         const gr3_seams_t *s) {
    /* ported sibling _under_authorization_profile composes inline; the
     * check lambda fires the authz-mixin collaborator seam. */
    return gr3_under_auth_profile(runner, source, "_admit_bot_message", s);
}

/* PoP: gr3_under_authorization_profile @ gateway/run.py:GatewayRunner._under_authorization_profile */
json_t *gr3_under_authorization_profile(const json_t *runner, const json_t *source,
                                        const gr3_seams_t *s) {
    return gr3_under_auth_profile(runner, source, "check", s);
}

/* PoP: gr3_session_env_scope @ gateway/run.py:GatewayRunner._session_env_scope */
/* CM flow driven as set -> body -> finally clear; an exit raise replaces
 * the body result/raise; a set raise skips body and clear. */
json_t *gr3_session_env_scope(const json_t *runner, const json_t *context,
                              const gr3_seams_t *s) {
    json_t *ta = gr3_args();
    gr3_j(ta, "context", context);
    json_t *tokens = gr3_runner_method(runner, "_set_session_env", ta, s);
    if (gr3_raised(tokens)) return tokens;
    json_t *body = gr3_call(s, "body", gr3_args());
    json_t *ca = gr3_args();
    gr3_j(ca, "tokens", tokens);
    json_free(tokens);
    json_t *cr = gr3_runner_method(runner, "_clear_session_env", ca, s);
    if (gr3_raised(cr)) { json_free(body); return cr; }
    json_free(cr);
    if (gr3_raised(body)) return body; /* body raise propagates through finally */
    json_free(body);
    return NULL; /* the CM drives the block; the flow ret is None */
}

/* PoP: gr3_get_housekeeping_executor @ gateway/run.py:GatewayRunner._get_housekeeping_executor */
json_t *gr3_get_housekeeping_executor(const json_t *runner, const gr3_seams_t *s) {
    json_t *a = gr3_args();
    gr3_j(a, "runner", runner);
    gr3_s(a, "attr", "_housekeeping_executor");
    gr3_s(a, "make_pool", "ThreadPoolExecutor");
    json_t *r = gr3_call(s, "_get_or_create_pool", a);
    if (gr3_raised(r)) return r;
    if (!r || r->type == JSON_NULL) {
        json_free(r);
        json_t *ca = gr3_args();
        gr3_n(ca, "max_workers", 4);
        gr3_s(ca, "thread_name_prefix", "hermes-gateway-hk");
        return gr3_call(s, "ThreadPoolExecutor", ca);
    }
    return r;
}

/* PoP: gr3_run_housekeeping_in_executor @ gateway/run.py:GatewayRunner._run_housekeeping_in_executor */
/* await is identity on the run_in_executor seam result; ported sibling
 * _get_housekeeping_executor composes inline. */
json_t *gr3_run_housekeeping_in_executor(const json_t *runner, const char *func,
                                         const json_t *args, const gr3_seams_t *s) {
    json_t *loop = gr3_call(s, "asyncio.get_running_loop", gr3_args());
    if (gr3_raised(loop)) return loop;
    json_t *exec = gr3_get_housekeeping_executor(runner, s);
    if (gr3_raised(exec)) { json_free(loop); return exec; }
    json_t *ctx = gr3_call(s, "copy_context", gr3_args());
    if (gr3_raised(ctx)) { json_free(loop); json_free(exec); return ctx; }
    json_free(ctx);
    json_t *a = gr3_args();
    gr3_j(a, "executor", exec);
    gr3_s(a, "fn", "copy_context.run");
    gr3_s(a, "func", func);
    gr3_j(a, "args", args);
    json_free(exec);
    json_free(loop);
    return gr3_call(s, "loop.run_in_executor", a);
}

/* PoP: gr3_migrate_command @ gateway/run.py:_migrate_command */
/* Live: `from hermes_cli.gateway_migrate import MIGRATE_COMMAND; return
 * MIGRATE_COMMAND`. The in-function import is the only I/O; it enters as the
 * MIGRATE_COMMAND seam. Its result is returned verbatim (value or raise spec,
 * including BaseException kinds which pass through every caller). */
json_t *gr3_migrate_command(const gr3_seams_t *s) {
    return gr3_call(s, "MIGRATE_COMMAND", gr3_args());
}
