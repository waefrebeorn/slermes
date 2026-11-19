/*
 * scheduler_flow2.c — FLOW-core port of the UNCLAIMED remainder of
 * cron/scheduler.py (LIVE authority). Sibling to src/cron/port_scheduler_flow.c
 * (scheduler_flow), which claims the tick/dispatch orchestration core. This
 * file ports the functions a PoP grep shows are still unclaimed: the in-flight
 * registry observers (running details / wedged keys / restart-wait counts /
 * external-worker record / latest releasable executions / claim-row fence),
 * the cron-interval cadence cache, the fallback-chain + gateway-sha probes,
 * the run-setup gates (prefill / preflight / blocked-config / credential pool
 * / MCP init / inactivity raise / run-doc header), the dotenv+delivery-target
 * publish, the fire secret-scope install/reset, the owned-run CAS, the fd
 * reclaim, and the throttled worktree-maintenance launch.
 *
 * NOTE on the module path: the task brief names "agent/scheduler.py", but the
 * quarry defines the cron scheduler in cron/scheduler.py (there is no
 * agent/scheduler.py). PoP annotations credit the REAL defining module
 * (cron/scheduler.py:<fn>) so the coverage scanner credits them.
 *
 * Self-contained C11, libjson-only. Every ported function is `static sc2_*`
 * and takes the universal seam io + a BORROWED json_t *in, returning a NEW
 * json_t* (caller frees): the value, or a {"__raise__":cls,"message":str}
 * raise spec. The harness #includes this file by area-qualified name
 * (agent/scheduler_flow2.c).
 *
 * SEAM DOCTRINE: every live effect (the clock, config/env reads, the jobs /
 * executions ledger, croniter, the fallback-chain helpers, the secret-scope
 * and contextvar collaborators, the delivery-target resolver, logging, and
 * EVERY claimed sibling — _job_route_pinned / _inflight_key /
 * _inflight_min_allowance_minutes / _job_interval_minutes / _get_hermes_home /
 * _hermes_now / _cron_preflight_enabled / _preflight_job_config /
 * _resolve_delivery_target — plus the unclaimed siblings composed for real:
 * _wedged_inflight_keys / _blocked_config_result) enters through ONE universal
 * seam:
 *
 *   json_t *(*call)(sc2_io_t *io, const char *name, json_t *args)
 *
 * `name` identifies the live call; `args` is a BORROWED json object (the seam
 * copies what it keeps; sc2_call frees nothing — sc2_seam frees args). The
 * result is a NEW json_t* the caller frees. NULL pointer == Python None. A
 * raised exception is {"__raise__": cls, "message": str, "errno": int?}.
 * sc2_call appends {"name","args"} to io->trace in call order BEFORE dispatch.
 *
 * Exception propagation: a seam raise is threaded up through the decision core
 * and returned as the function result unless a live except/suppress clause
 * catches it. `except Exception` / `suppress(Exception)` swallow the Exception
 * family but the BaseException trio (SystemExit / KeyboardInterrupt /
 * GeneratorExit) ALWAYS passes through (full builtin parent table below).
 * Fail-closed: a missing/exhausted seam script yields a RuntimeError raise
 * spec (recorded, then propagated/swallowed exactly as live).
 *
 * Representation contract (shared with the oracle):
 * - In-flight KEY = (home_key, job_id) tuple -> JSON 2-array [home_key, job_id].
 * - Module-global sets (_running_job_ids / _scope_isolated_job_ids) -> JSON
 *   arrays of keys in INSERTION (fixture) order; live set iteration order is
 *   hash-randomized and NOT part of the contract. Membership / len / sorted
 *   projections are order-insensitive; sorted outputs are sorted on BOTH sides.
 * - Module-global dicts keyed by an in-flight key (_running_since /
 *   _running_allowance_s / _running_worker_pids / _running_futures /
 *   _running_fire_owners) -> JSON arrays of {"key":[h,j], <field>:value}
 *   entries in insertion order (a JSON object cannot carry a tuple key).
 *   _running_fire_owners entries carry {"key","owners"}; only .keys() is read.
 * - String-keyed dicts (_inflight_home_paths / _cron_interval_cache /
 *   _last_worktree_maintenance_at) -> JSON objects.
 * - Paths are POSIX strings (PurePosixPath model; clean-path fixture domain).
 * - Datetimes travel as ISO-8601 strings; aware timestamps compare by epoch.
 *   _hermes_now().strftime("%Y-%m-%d %H:%M:%S") is the only strftime rendered.
 * - future spec: null (None), {"__pending__":true} (_FUTURE_PENDING), or
 *   {"id":str} (a real future whose .done() is the "future.done" seam).
 * - pool spec: {"has_credentials":bool,"entries":[...]} (methods are pure reads).
 * - agent spec: {"id":str,"has_activity":bool}; token/scope markers are opaque
 *   scripted JSON values returned verbatim.
 * - logger calls ARE traced as "logger.<level>" seams with
 *   {msg, args:[projected positionals], exc_info:true?}; BaseException args
 *   project to str(exc), Path args to str. exc_info=True carries args:[].
 * - Integral floats == ints (libjson has no int/float identity); str()-position
 *   and type-name fixtures stay integral-int or non-integral (documented).
 * - int()/fromisoformat() invalid-token fixtures stay printable ASCII without
 *   quotes/backslashes (the messages repr-quote the token).
 * - Scripted raises use single-message builtin kinds (no KeyError scripts;
 *   live KeyError messages are pre-repr'd by the port).
 *
 * Import-failure gates: in.import_fail is a list of module full names; a gated
 * `from mod import name` raises ImportError BEFORE any seam fires (modelled by
 * skipping the seam). The live `with suppress(Exception)` / `except Exception`
 * then swallows it exactly as a call raise would, but with NO trace event.
 *
 * Seam name table (name -> live call):
 *   get_fallback_chain            get_fallback_chain(cfg)
 *   _job_route_pinned             _job_route_pinned(job)            [claimed]
 *   scoped_fallback_chain         scoped_fallback_chain(inherited,declared,pinned=,owner=)
 *   current_code_sha              gateway.code_skew.current_code_sha()
 *   hermes_home_key               hermes_constants.hermes_home_key(home)
 *   _get_hermes_home              _get_hermes_home() -> Path        [claimed]
 *   get_hermes_home               hermes_constants.get_hermes_home() -> Path
 *   time.time / time.monotonic    clock reads
 *   _inflight_min_allowance_minutes  floor allowance (min)          [claimed]
 *   _job_interval_minutes         _job_interval_minutes(job)        [claimed]
 *   load_jobs                     cron.jobs.load_jobs() -> [job]
 *   _inflight_key                 _inflight_key(job_id, home)       [claimed]
 *   _ensure_croniter              cron.jobs._ensure_croniter() -> bool
 *   croniter                      cron.jobs.croniter(expr, base) -> iterator
 *   croniter.get_next             iterator.get_next(datetime) -> ISO str
 *   datetime.now                  datetime.now() -> ISO str
 *   future.done                   future.done() -> bool
 *   latest_executions             cron.executions.latest_executions(job_ids)
 *   _ensure_aware                 cron.jobs._ensure_aware(dt) -> aware ISO str
 *   gc.collect                    gc.collect()
 *   apply_nofile_soft_limit       resource_limits.apply_nofile_soft_limit(None)
 *   cron_env_setting              cron.env_settings.cron_env_setting(name)
 *   path.expanduser               Path.expanduser() -> str
 *   path.exists                   Path.exists() -> bool
 *   prefill.load                  open(path,utf-8-sig)+json.load -> value
 *   _cron_preflight_enabled       _cron_preflight_enabled(cfg)      [claimed]
 *   _preflight_job_config         _preflight_job_config(job,cfg)    [claimed]
 *   clear_preflight_alerted       cron.jobs.clear_preflight_alerted(job_id)
 *   mark_preflight_alerted        cron.jobs.mark_preflight_alerted(job_id)
 *   _hermes_now                   hermes_time.now() -> aware ISO    [claimed]
 *   load_route_pool               agent.custom_entry_pool.load_route_pool(...)
 *   discover_mcp_tools            tools.mcp_tool_discovery.discover_mcp_tools()
 *   agent.get_activity_summary    agent.get_activity_summary() -> dict
 *   request_hard_interrupt        request_hard_interrupt(agent,reason,tool_reason=)
 *   reset_secret_source_cache     env_loader.reset_secret_source_cache(home)
 *   load_hermes_dotenv            env_loader.load_hermes_dotenv(hermes_home=)
 *   _resolve_delivery_target      _resolve_delivery_target(job)     [claimed]
 *   var_map.set                   _VAR_MAP[name].set(value)
 *   hydrate_profile_secret_sources env_loader.hydrate_profile_secret_sources(home)
 *   owner_secret_scope            launch_profile_policy.owner_secret_scope(home)
 *   set_secret_scope              secret_scope.set_secret_scope(scope,profile_home=)
 *   routed_profile_fire           scheduler_provider.routed_profile_fire()
 *   set_multiplex_context         secret_scope.set_multiplex_context(True)
 *   reset_multiplex_context       secret_scope.reset_multiplex_context(token)
 *   reset_secret_scope            secret_scope.reset_secret_scope(token)
 *   os.environ.get                os.environ.get(key)
 *   get_execution                 cron.executions.get_execution(execution_id)
 *   mark_execution_running        cron.executions.mark_execution_running(execution_id)
 *   enter_cron_execution          execution_identity.enter_cron_execution(job,eid,record)
 *   thread.start                  threading.Thread(...).start()
 *   run_sweep                     scheduler_worktree_maintenance.run_sweep(repos,interval_seconds=)
 *   logger.<level>                logger.<level>(msg,*args,exc_info=?)
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to cron/scheduler.py + split mods) ── */
#define SC2_SILENT_MARKER "[SILENT]"
#define SC2_BLOCKED_CONFIG_MARKER "[blocked_config]"
#define SC2_BLOCKED_CONFIG_SILENT_MARKER "[blocked_config:silent]"
#define SC2_PROMPT_HEADING "## Prompt\n\n"
#define SC2_PROMPT_SEPARATOR "\n\n"
#define SC2_WORKTREE_MAINTENANCE_INTERVAL_SECONDS 21600.0  /* 6 * 3600 */

/* ── universal seam plumbing ─────────────────────────────────────────────── */
typedef struct sc2_io_t {
    json_t *(*call)(struct sc2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ordered array of {"name","args"} */
    json_t *state;   /* working module-global state object (harness-owned) */
    void   *ctx;
} sc2_io_t;

static json_t *sc2_call(sc2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}
/* Call a seam, free args, return an owned result (NULL normalized to null). */
static json_t *sc2_seam(sc2_io_t *io, const char *name, json_t *args) {
    json_t *r = sc2_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}
static bool sc2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *sc2_raise(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message ? message : "");
    return o;
}
static json_t *sc2_raise_errno(const char *type, const char *message, double en) {
    json_t *o = sc2_raise(type, message);
    json_set(o, "errno", json_number(en));
    return o;
}

/* ── Python truthiness / type tests ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *jstr(const json_t *v) { return j_is_str(v) ? v->str_val : ""; }
static double jnum(const json_t *v) { return j_is_num(v) ? v->num_val : 0.0; }
static bool jbool(const json_t *v) { return j_is_bool(v) && v->bool_val; }
/* `is True` identity gate (JSON_BOOL true only). */
static bool j_is_true(const json_t *v) { return j_is_bool(v) && v->bool_val; }
/* `is False` identity gate. */
static bool j_is_false(const json_t *v) { return j_is_bool(v) && !v->bool_val; }
/* Copy a borrowed node into an owned arg value; absent (NULL) -> explicit JSON
 * null (json_set(obj,key,NULL) is a NO-OP in libjson, which would drop the key
 * and diverge from a live present-null kwarg). */
static json_t *sc2_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* ── string builder ──────────────────────────────────────────────────────── */
typedef struct { char *s; size_t len, cap; } sc2_sb;
static void sb_init(sc2_sb *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); b->s[0] = '\0'; }
static void sb_add(sc2_sb *b, const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->s = realloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, s, n); b->len += n; b->s[b->len] = '\0';
}
static void sb_addch(sc2_sb *b, char c) { char t[2] = {c, '\0'}; sb_add(b, t); }
static char *sb_take(sc2_sb *b) { char *s = b->s; b->s = NULL; b->len = b->cap = 0; return s; }

/* ── exception classification (FULL builtin hierarchy) ───────────────────── */
static const char *exc_type(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static const struct { const char *name, *parent; } SC2_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
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
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
};
static const char *exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(SC2_EXC_PARENTS)/sizeof(SC2_EXC_PARENTS[0]); i++)
        if (strcmp(SC2_EXC_PARENTS[i].name, kind) == 0) return SC2_EXC_PARENTS[i].parent;
    return NULL;
}
/* True when `kind` is `target` or a subclass of it (walks the parent table). */
static bool exc_kind_is(const char *kind, const char *target) {
    const char *k = kind;
    int guard = 0;
    while (k && guard++ < 32) {
        if (strcmp(k, target) == 0) return true;
        k = exc_parent(k);
    }
    return false;
}
static bool exc_is(const json_t *r, const char *target) { return exc_kind_is(exc_type(r), target); }
static bool exc_is_exception(const json_t *r) { return exc_is(r, "Exception"); }
/* str(exc) parity: OSError(errno,msg) -> "[Errno N] msg"; else message. */
static char *exc_str(const json_t *r) {
    const char *msg = json_get_str(r, "message", "");
    json_t *en = json_obj_get(r, "errno");
    size_t cap = strlen(msg) + 48;
    char *out = malloc(cap);
    if (j_is_num(en)) snprintf(out, cap, "[Errno %.0f] %s", en->num_val, msg);
    else snprintf(out, cap, "%s", msg);
    return out;
}

/* ── Python type names / str() / repr() ──────────────────────────────────── */
static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && isfinite(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* Python str() for the fixture domain (null/bool/str/integral-int). */
static char *py_str(const json_t *v) {
    if (j_is_null(v)) return strdup("None");
    if (j_is_bool(v)) return strdup(v->bool_val ? "True" : "False");
    if (j_is_str(v)) return strdup(v->str_val ? v->str_val : "");
    if (j_is_num(v)) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && isfinite(v->num_val) && fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    return ser ? ser : strdup("");
}
/* repr() of a str: Python picks double quotes when the string contains a
 * single quote and no double quote; otherwise single quotes. Escapes \\, the
 * chosen quote, and control chars. */
static char *py_repr_str(const char *s) {
    s = s ? s : "";
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    sc2_sb b; sb_init(&b);
    sb_addch(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)q) { sb_addch(&b, '\\'); sb_addch(&b, (char)c); }
        else if (c == '\n') sb_add(&b, "\\n");
        else if (c == '\r') sb_add(&b, "\\r");
        else if (c == '\t') sb_add(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char t[8]; snprintf(t, sizeof t, "\\x%02x", c); sb_add(&b, t); }
        else sb_addch(&b, (char)c);
    }
    sb_addch(&b, q);
    return sb_take(&b);
}

/* ── numeric helpers ─────────────────────────────────────────────────────── */
/* round(x, 1) == float("%.1f" % x) (probed: 0 diffs over 20k random). */
static double sc2_round1(double x) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.1f", x);
    return strtod(buf, NULL);
}
/* int(v) coercion. Returns true + *out on success; false + *err (raise spec). */
static bool sc2_py_int(const json_t *v, long long *out, json_t **err) {
    *err = NULL;
    if (j_is_bool(v)) { *out = v->bool_val ? 1 : 0; return true; }
    if (j_is_num(v)) {
        if (!isfinite(v->num_val)) {
            *err = sc2_raise(isinf(v->num_val) ? "OverflowError" : "ValueError",
                             isinf(v->num_val) ? "cannot convert float infinity to integer"
                                               : "cannot convert float NaN to integer");
            return false;
        }
        *out = (long long)v->num_val;  /* trunc toward zero */
        return true;
    }
    if (j_is_str(v)) {
        const char *s = v->str_val ? v->str_val : "";
        /* int() strips ASCII whitespace (fixture domain). */
        size_t a = 0, b = strlen(s);
        while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\v'||s[a]=='\f')) a++;
        while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\v'||s[b-1]=='\f')) b--;
        size_t i = a;
        if (i < b && (s[i]=='+' || s[i]=='-')) i++;
        size_t digstart = i;
        bool any = false, prev_digit = false, bad = false;
        for (; i < b; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') { any = true; prev_digit = true; }
            else if (c == '_') { if (!prev_digit) { bad = true; break; } prev_digit = false; }
            else { bad = true; break; }
        }
        if (bad || !any || !prev_digit) {
            char *r = py_repr_str(s);
            size_t cap = strlen(r) + 64;
            char *msg = malloc(cap);
            snprintf(msg, cap, "invalid literal for int() with base 10: %s", r);
            *err = sc2_raise("ValueError", msg);
            free(r); free(msg);
            return false;
        }
        /* clean buffer: sign + digits without underscores */
        char *clean = malloc(b - a + 2);
        size_t k = 0;
        if (digstart > a) clean[k++] = s[a];  /* sign */
        for (size_t j = digstart; j < b; j++) if (s[j] != '_') clean[k++] = s[j];
        clean[k] = '\0';
        *out = strtoll(clean, NULL, 10);
        free(clean);
        return true;
    }
    /* null / dict / list -> TypeError */
    const char *tn = py_type_name(v);
    size_t cap = strlen(tn) + 96;
    char *msg = malloc(cap);
    snprintf(msg, cap, "int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    *err = sc2_raise("TypeError", msg);
    free(msg);
    return false;
}

/* ── ISO-8601 datetime helpers (aware; epoch arithmetic) ─────────────────── */
typedef struct { int y, mo, d, h, mi, s; double frac; long off; bool ok; } sc2_dt;
static long sc2_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static bool sc2_is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
/* Parse YYYY-MM-DD[T ]HH:MM:SS[.ffffff][Z|+HH:MM|-HH:MM]. Full date+time required. */
static sc2_dt sc2_iso_parse(const char *s) {
    sc2_dt dt = {0,0,0,0,0,0,0.0,0,false};
    if (!s) return dt;
    int y, mo, d, h, mi, sec; char sep;
    int consumed = 0;
    if (sscanf(s, "%4d-%2d-%2d%c%2d:%2d:%2d%n", &y, &mo, &d, &sep, &h, &mi, &sec, &consumed) < 7)
        return dt;
    if (sep != 'T' && sep != ' ' && sep != 't') return dt;
    if (mo < 1 || mo > 12 || d < 1 || h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 59)
        return dt;
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxd = mdays[mo-1] + ((mo == 2 && sc2_is_leap(y)) ? 1 : 0);
    if (d > maxd) return dt;
    size_t i = (size_t)consumed;
    double frac = 0.0;
    if (s[i] == '.') {
        i++;
        double scale = 0.1;
        bool any = false;
        while (s[i] >= '0' && s[i] <= '9') { frac += (s[i]-'0') * scale; scale *= 0.1; i++; any = true; }
        if (!any) return dt;
    }
    long off = 0;
    if (s[i] == 'Z' || s[i] == 'z') { i++; }
    else if (s[i] == '+' || s[i] == '-') {
        int sign = s[i] == '+' ? 1 : -1; i++;
        int oh, om;
        if (sscanf(s + i, "%2d:%2d", &oh, &om) < 2) return dt;
        if (oh < 0 || oh > 23 || om < 0 || om > 59) return dt;
        off = sign * (oh * 3600 + om * 60);
        i += 5;
    }
    if (s[i] != '\0') return dt;
    dt.y = y; dt.mo = mo; dt.d = d; dt.h = h; dt.mi = mi; dt.s = sec;
    dt.frac = frac; dt.off = off; dt.ok = true;
    return dt;
}
static double sc2_dt_epoch(const sc2_dt *dt) {
    return (double)(sc2_days_from_civil(dt->y, dt->mo, dt->d) * 86400LL
                    + dt->h * 3600 + dt->mi * 60 + dt->s) + dt->frac - (double)dt->off;
}
/* strftime("%Y-%m-%d %H:%M:%S") on a scripted ISO string (NULL on bad parse). */
static char *sc2_iso_strftime(const char *s) {
    sc2_dt dt = sc2_iso_parse(s);
    if (!dt.ok) return NULL;
    char buf[64];
    snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d", dt.y, dt.mo, dt.d, dt.h, dt.mi, dt.s);
    return strdup(buf);
}

/* ── POSIX path join (PurePosixPath / ; clean-path domain) ───────────────── */
static char *sc2_posix_join(const char *a, const char *b) {
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    if (!a || !a[0]) return strdup(b);
    size_t na = strlen(a);
    bool slash = a[na-1] == '/';
    size_t n = na + strlen(b) + 2;
    char *out = malloc(n);
    snprintf(out, n, "%s%s%s", a, slash ? "" : "/", b);
    return out;
}

/* ── in-flight key helpers (key = [home_key, job_id]) ────────────────────── */
static json_t *sc2_mk_key(const char *h, const char *j) {
    json_t *k = json_array();
    json_append(k, json_string(h ? h : ""));
    json_append(k, json_string(j ? j : ""));
    return k;
}
static const char *sc2_key_home(const json_t *k) { return jstr(json_get(k, 0)); }
static const char *sc2_key_job(const json_t *k) { return jstr(json_get(k, 1)); }
static bool sc2_key_eq(const json_t *a, const json_t *b) {
    if (!j_is_arr(a) || !j_is_arr(b) || json_len(a) != 2 || json_len(b) != 2) return false;
    return strcmp(sc2_key_home(a), sc2_key_home(b)) == 0
        && strcmp(sc2_key_job(a), sc2_key_job(b)) == 0;
}
static bool sc2_arr_has_key(const json_t *arr, const json_t *k) {
    if (!j_is_arr(arr)) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sc2_key_eq(json_get(arr, i), k)) return true;
    return false;
}
static int sc2_key_cmp(const void *pa, const void *pb) {
    const json_t *a = *(const json_t * const *)pa;
    const json_t *b = *(const json_t * const *)pb;
    int c = strcmp(sc2_key_home(a), sc2_key_home(b));
    if (c) return c;
    return strcmp(sc2_key_job(a), sc2_key_job(b));
}
/* sorted copy of an array of keys. */
static json_t *sc2_sorted_keys(const json_t *arr) {
    json_t *out = json_array();
    if (!j_is_arr(arr) || json_len(arr) == 0) return out;
    size_t n = json_len(arr);
    json_t **tmp = malloc(n * sizeof(json_t *));
    for (size_t i = 0; i < n; i++) tmp[i] = json_get((json_t *)arr, i);
    qsort(tmp, n, sizeof(json_t *), sc2_key_cmp);
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(tmp[i]));
    free(tmp);
    return out;
}
/* sorted unique job-id projection of an array of keys. */
static json_t *sc2_sorted_job_ids(const json_t *arr) {
    json_t *out = json_array();
    if (!j_is_arr(arr)) return out;
    size_t n = json_len(arr);
    const char **ids = malloc((n ? n : 1) * sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const char *j = sc2_key_job(json_get(arr, i));
        bool dup = false;
        for (size_t k = 0; k < m; k++) if (strcmp(ids[k], j) == 0) { dup = true; break; }
        if (!dup) ids[m++] = j;
    }
    /* insertion-sort the small id set by strcmp */
    for (size_t i = 1; i < m; i++) {
        const char *v = ids[i]; size_t k = i;
        while (k > 0 && strcmp(ids[k-1], v) > 0) { ids[k] = ids[k-1]; k--; }
        ids[k] = v;
    }
    for (size_t i = 0; i < m; i++) json_append(out, json_string(ids[i]));
    free(ids);
    return out;
}
/* union of a key-array and the keys of an entry-array ({"key":...}); first-wins. */
static json_t *sc2_keys_union(const json_t *keys, const json_t *entries) {
    json_t *out = json_array();
    if (j_is_arr(keys))
        for (size_t i = 0; i < json_len(keys); i++) json_append(out, json_copy(json_get(keys, i)));
    if (j_is_arr(entries))
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *k = json_obj_get(json_get(entries, i), "key");
            if (k && !sc2_arr_has_key(out, k)) json_append(out, json_copy(k));
        }
    return out;
}

/* ── key-keyed dict-model helpers (array of {"key":[h,j], field:value}) ──── */
static json_t *sc2_dm_entry(const json_t *arr, const json_t *k) {
    if (!j_is_arr(arr)) return NULL;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (sc2_key_eq(json_obj_get(e, "key"), k)) return e;
    }
    return NULL;
}
static json_t *sc2_dm_get(const json_t *arr, const json_t *k, const char *field) {
    json_t *e = sc2_dm_entry(arr, k);
    return e ? json_obj_get(e, field) : NULL;
}
static void sc2_dm_set(json_t *arr, const json_t *k, const char *field, json_t *val) {
    json_t *e = sc2_dm_entry(arr, k);
    if (e) { json_set(e, field, val); return; }
    e = json_object();
    json_set(e, "key", json_copy(k));
    json_set(e, field, val);
    json_append(arr, e);
}

/* ── state accessors ─────────────────────────────────────────────────────── */
static json_t *sc2_st(sc2_io_t *io, const char *name) { return json_obj_get(io->state, name); }
static json_t *sc2_st_ensure_arr(sc2_io_t *io, const char *name) {
    json_t *v = json_obj_get(io->state, name);
    if (j_is_arr(v)) return v;
    v = json_array();
    json_set(io->state, name, v);
    return v;
}
static json_t *sc2_st_ensure_obj(sc2_io_t *io, const char *name) {
    json_t *v = json_obj_get(io->state, name);
    if (j_is_obj(v)) return v;
    v = json_object();
    json_set(io->state, name, v);
    return v;
}

/* ── import-failure gate (in.import_fail list) ───────────────────────────── */
static bool sc2_import_fail(json_t *in, const char *mod) {
    json_t *lst = json_obj_get(in, "import_fail");
    if (!j_is_arr(lst)) return false;
    for (size_t i = 0; i < json_len(lst); i++)
        if (strcmp(jstr(json_get(lst, i)), mod) == 0) return true;
    return false;
}

/* ── logger seam (msg + projected positional args + exc_info) ────────────── */
/* args is OWNED (freed here). exc_info: 0 omit, 1 include true. */
static void sc2_log(sc2_io_t *io, const char *level, const char *msg, json_t *args, int exc_info) {
    json_t *a = json_object();
    json_set_hdr(a, "msg", msg);
    json_set(a, "args", args ? args : json_array());
    if (exc_info) json_set(a, "exc_info", json_bool(true));
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = sc2_seam(io, name, a);
    if (r) json_free(r);
}
/* logger arg node projecting a raise spec to str(exc). */
static json_t *sc2_exc_arg(const json_t *r) {
    char *s = exc_str(r);
    json_t *n = json_string(s);
    free(s);
    return n;
}
/* ── composed-sibling prototypes ─────────────────────────────────────────── */
static json_t *sc2_wedged_inflight_keys(sc2_io_t *io, json_t *in);
static json_t *sc2_blocked_config_result(sc2_io_t *io, json_t *in);

/* Normalize a parsed dt to datetime.isoformat() (aware; +HH:MM offset). */
static char *sc2_dt_isoformat(const sc2_dt *dt) {
    sc2_sb b; sb_init(&b);
    char t[64];
    snprintf(t, sizeof t, "%04d-%02d-%02dT%02d:%02d:%02d", dt->y, dt->mo, dt->d, dt->h, dt->mi, dt->s);
    sb_add(&b, t);
    if (dt->frac > 0.0) {
        long us = (long)(dt->frac * 1e6 + 0.5);
        if (us > 0) { snprintf(t, sizeof t, ".%06ld", us); sb_add(&b, t); }
    }
    long off = dt->off;
    char sign = off < 0 ? '-' : '+';
    if (off < 0) off = -off;
    snprintf(t, sizeof t, "%c%02ld:%02ld", sign, off / 3600, (off % 3600) / 60);
    sb_add(&b, t);
    return sb_take(&b);
}

/* Suppress(Exception): free an Exception-family raise (return NULL); a
 * BaseException raise is returned for propagation. Non-raise -> NULL. */
static json_t *sc2_swallow_exception(json_t *r) {
    if (!sc2_raised(r)) return NULL;
    if (exc_is_exception(r)) { json_free(r); return NULL; }
    return r;
}

/* PoP: sc2_job_fallback_chain @ cron/scheduler.py:_job_fallback_chain */
static json_t *sc2_job_fallback_chain(sc2_io_t *io, json_t *in) {
    /* return scoped_fallback_chain(get_fallback_chain(cfg), None,
     *                              pinned=_job_route_pinned(job), owner="cron job") */
    json_t *a1 = json_object();
    json_set(a1, "cfg", sc2_copy_or_null(json_obj_get(in, "cfg")));
    json_t *chain = sc2_seam(io, "get_fallback_chain", a1);
    if (sc2_raised(chain)) return chain;
    json_t *a2 = json_object();
    json_set(a2, "job", sc2_copy_or_null(json_obj_get(in, "job")));
    json_t *pinned = sc2_seam(io, "_job_route_pinned", a2);
    if (sc2_raised(pinned)) { json_free(chain); return pinned; }
    json_t *a3 = json_object();
    json_set(a3, "inherited", chain);            /* ownership transferred */
    json_set(a3, "declared", json_null());
    json_set(a3, "pinned", pinned);              /* ownership transferred */
    json_set_hdr(a3, "owner", "cron job");
    json_t *r = sc2_seam(io, "scoped_fallback_chain", a3);
    return r;
}

/* PoP: sc2_current_gateway_code_sha @ cron/scheduler.py:_current_gateway_code_sha */
static json_t *sc2_current_gateway_code_sha(sc2_io_t *io, json_t *in) {
    /* try: from gateway.code_skew import current_code_sha; return current_code_sha()
     * except Exception: return None */
    if (sc2_import_fail(in, "gateway.code_skew")) return json_null();
    json_t *r = sc2_seam(io, "current_code_sha", json_object());
    if (sc2_raised(r)) {
        json_t *p = sc2_swallow_exception(r);
        if (p) return p;
        return json_null();
    }
    return r;
}

/* PoP: sc2_remember_inflight_home @ cron/scheduler.py:_remember_inflight_home */
static json_t *sc2_remember_inflight_home(sc2_io_t *io, json_t *in) {
    /* _inflight_home_paths[hermes_home_key(home)] = home; return home */
    const char *home = json_get_str(in, "home", "");
    json_t *a = json_object();
    json_set_hdr(a, "home", home);
    json_t *key = sc2_seam(io, "hermes_home_key", a);
    if (sc2_raised(key)) return key;
    json_t *paths = sc2_st_ensure_obj(io, "_inflight_home_paths");
    json_set(paths, jstr(key), json_string(home));
    json_free(key);
    return json_string(home);
}

/* PoP: sc2_inflight_home_path @ cron/scheduler.py:_inflight_home_path */
static json_t *sc2_inflight_home_path(sc2_io_t *io, json_t *in) {
    /* return _inflight_home_paths.get(home_key) or Path(home_key) */
    const char *hk = json_get_str(in, "home_key", "");
    json_t *paths = sc2_st(io, "_inflight_home_paths");
    json_t *v = paths ? json_obj_get(paths, hk) : NULL;
    /* Path object presence (always truthy when present-non-null), not str truth. */
    if (v && v->type != JSON_NULL) return json_copy(v);
    return json_string(hk[0] ? hk : ".");  /* PurePosixPath("") == "." */
}

/* PoP: sc2_get_running_job_details @ cron/scheduler.py:get_running_job_details */
static json_t *sc2_get_running_job_details(sc2_io_t *io, json_t *in) {
    (void)in;
    json_t *now = sc2_seam(io, "time.time", json_object());
    if (sc2_raised(now)) return now;
    double t = jnum(now);
    json_free(now);
    json_t *ids = sc2_st(io, "_running_job_ids");
    json_t *owners = sc2_st(io, "_running_fire_owners");
    json_t *uni = sc2_keys_union(ids, owners);
    json_t *sorted = sc2_sorted_keys(uni);
    json_free(uni);
    json_t *since = sc2_st(io, "_running_since");
    json_t *pids = sc2_st(io, "_running_worker_pids");
    json_t *scope = sc2_st(io, "_scope_isolated_job_ids");
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(sorted); i++) {
        json_t *k = json_get(sorted, i);
        json_t *row = json_object();
        json_set_hdr(row, "job_id", sc2_key_job(k));
        json_t *st = sc2_dm_get(since, k, "started");
        if (st) json_set(row, "elapsed_s", json_number(sc2_round1(t - jnum(st))));
        else json_set(row, "elapsed_s", json_null());
        json_t *pid = sc2_dm_get(pids, k, "pid");
        json_set(row, "worker_pid", pid ? json_copy(pid) : json_null());
        json_set(row, "restart_safe", json_bool(sc2_arr_has_key(scope, k)));
        json_append(out, row);
    }
    json_free(sorted);
    return out;
}

/* PoP: sc2_wedged_inflight_keys @ cron/scheduler.py:_wedged_inflight_keys */
static json_t *sc2_wedged_inflight_keys(sc2_io_t *io, json_t *in) {
    json_t *now = sc2_seam(io, "time.time", json_object());
    if (sc2_raised(now)) return now;
    double t = jnum(now);
    json_free(now);
    json_t *ids = sc2_st(io, "_running_job_ids");
    json_t *since = sc2_st(io, "_running_since");
    json_t *allow_state = sc2_st(io, "_running_allowance_s");
    /* ages = {key: now-started for key,started in _running_since.items()
     *         if key in _running_job_ids}  (insertion order) */
    json_t *ages = json_array();
    if (j_is_arr(since)) {
        for (size_t i = 0; i < json_len(since); i++) {
            json_t *e = json_get(since, i);
            json_t *k = json_obj_get(e, "key");
            if (k && sc2_arr_has_key(ids, k)) {
                json_t *a = json_object();
                json_set(a, "key", json_copy(k));
                json_set(a, "age", json_number(t - jnum(json_obj_get(e, "started"))));
                json_append(ages, a);
            }
        }
    }
    if (json_len(ages) == 0) { json_free(ages); return json_array(); }
    /* allowances = {key: _running_allowance_s[key] for key in ages if present} */
    json_t *allows = json_array();
    for (size_t i = 0; i < json_len(ages); i++) {
        json_t *k = json_obj_get(json_get(ages, i), "key");
        json_t *av = sc2_dm_get(allow_state, k, "allowance");
        if (av) {
            json_t *a = json_object();
            json_set(a, "key", json_copy(k));
            json_set(a, "allowance", json_copy(av));
            json_append(allows, a);
        }
    }
    /* floor_seconds = _inflight_min_allowance_minutes() * 60.0  (raise propagates) */
    json_t *fl = sc2_seam(io, "_inflight_min_allowance_minutes", json_object());
    if (sc2_raised(fl)) { json_free(ages); json_free(allows); return fl; }
    double floor_s = jnum(fl) * 60.0;
    json_free(fl);
    /* unresolved = [key for key in ages if key not in allowances] */
    json_t *unresolved = json_array();
    for (size_t i = 0; i < json_len(ages); i++) {
        json_t *k = json_obj_get(json_get(ages, i), "key");
        if (!sc2_dm_entry(allows, k)) json_append(unresolved, json_copy(k));
    }
    if (json_len(unresolved) > 0) {
        /* by_id = {}; with suppress(Exception): from cron.jobs import load_jobs;
         * by_id = {j.get("id"): j for j in load_jobs()}  (atomic comprehension) */
        json_t *by_id = json_object();
        json_t *prop = NULL;
        do {
            if (sc2_import_fail(in, "cron.jobs")) break;  /* ImportError swallowed */
            json_t *lj = sc2_seam(io, "load_jobs", json_object());
            if (sc2_raised(lj)) { prop = sc2_swallow_exception(lj); break; }
            if (!j_is_arr(lj)) {
                /* iterating a non-list raises TypeError -> swallowed */
                json_free(lj); break;
            }
            json_t *tmp = json_object();
            bool comp_ok = true;
            for (size_t i = 0; i < json_len(lj); i++) {
                json_t *j = json_get(lj, i);
                if (!j_is_obj(j)) { comp_ok = false; break; }  /* j.get -> AttributeError */
                json_set(tmp, jstr(json_obj_get(j, "id")), json_copy(j));
            }
            json_free(lj);
            if (comp_ok) { json_free(by_id); by_id = tmp; }
            else json_free(tmp);
        } while (0);
        if (prop) { json_free(by_id); json_free(ages); json_free(allows); json_free(unresolved); return prop; }
        /* local_home = hermes_home_key(_get_hermes_home())  (raise propagates) */
        json_t *hh = sc2_seam(io, "_get_hermes_home", json_object());
        if (sc2_raised(hh)) { json_free(by_id); json_free(ages); json_free(allows); json_free(unresolved); return hh; }
        json_t *a = json_object();
        json_set(a, "home", hh);  /* ownership transferred */
        json_t *lh = sc2_seam(io, "hermes_home_key", a);
        if (sc2_raised(lh)) { json_free(by_id); json_free(ages); json_free(allows); json_free(unresolved); return lh; }
        const char *local_home = jstr(lh);
        /* for key in unresolved: resolve allowance, write state if still running */
        json_t *allow_w = sc2_st_ensure_arr(io, "_running_allowance_s");
        for (size_t i = 0; i < json_len(unresolved); i++) {
            json_t *k = json_get(unresolved, i);
            double allowance = floor_s;
            json_t *local_job = NULL;
            if (strcmp(sc2_key_home(k), local_home) == 0)
                local_job = json_obj_get(by_id, sc2_key_job(k));
            json_t *argjob = (local_job && jtruth(local_job)) ? json_copy(local_job) : json_object();
            json_t *aa = json_object();
            json_set(aa, "job", argjob);
            json_t *iv = sc2_seam(io, "_job_interval_minutes", aa);
            if (sc2_raised(iv)) {
                json_free(lh); json_free(by_id); json_free(ages); json_free(allows); json_free(unresolved);
                return iv;
            }
            if (jtruth(iv)) {
                double cand = 2.0 * jnum(iv) * 60.0;
                if (cand > allowance) allowance = cand;
            }
            json_free(iv);
            sc2_dm_set(allows, k, "allowance", json_number(allowance));
            if (sc2_arr_has_key(ids, k))
                sc2_dm_set(allow_w, k, "allowance", json_number(allowance));
        }
        json_free(lh);
        json_free(by_id);
    }
    json_free(unresolved);
    /* return frozenset(key for key,age in ages if age >= max(allowance, floor)) */
    json_t *wedged = json_array();
    for (size_t i = 0; i < json_len(ages); i++) {
        json_t *e = json_get(ages, i);
        json_t *k = json_obj_get(e, "key");
        double age = jnum(json_obj_get(e, "age"));
        double al = jnum(sc2_dm_get(allows, k, "allowance"));
        double bound = al > floor_s ? al : floor_s;
        if (age >= bound) json_append(wedged, json_copy(k));
    }
    json_free(ages);
    json_free(allows);
    json_t *sorted = sc2_sorted_keys(wedged);
    json_free(wedged);
    return sorted;
}

/* PoP: sc2_get_wedged_job_ids @ cron/scheduler.py:get_wedged_job_ids */
static json_t *sc2_get_wedged_job_ids(sc2_io_t *io, json_t *in) {
    /* return frozenset(key[1] for key in _wedged_inflight_keys()) */
    json_t *keys = sc2_wedged_inflight_keys(io, in);
    if (sc2_raised(keys)) return keys;
    json_t *out = sc2_sorted_job_ids(keys);
    json_free(keys);
    return out;
}

/* PoP: sc2_get_restart_wait_cron_counts @ cron/scheduler.py:get_restart_wait_cron_counts */
static json_t *sc2_get_restart_wait_cron_counts(sc2_io_t *io, json_t *in) {
    json_t *wedged = sc2_wedged_inflight_keys(io, in);
    if (sc2_raised(wedged)) return wedged;
    json_t *ids = sc2_st(io, "_running_job_ids");
    json_t *owners = sc2_st(io, "_running_fire_owners");
    json_t *scope = sc2_st(io, "_scope_isolated_job_ids");
    json_t *inflight = sc2_keys_union(ids, owners);
    /* restart_safe = (_scope_isolated_job_ids & inflight) - wedged */
    json_t *restart_safe = json_array();
    if (j_is_arr(scope)) {
        for (size_t i = 0; i < json_len(scope); i++) {
            json_t *k = json_get(scope, i);
            if (sc2_arr_has_key(inflight, k) && !sc2_arr_has_key(wedged, k))
                json_append(restart_safe, json_copy(k));
        }
    }
    /* wedged = wedged & inflight */
    json_t *w2 = json_array();
    for (size_t i = 0; i < json_len(wedged); i++) {
        json_t *k = json_get(wedged, i);
        if (sc2_arr_has_key(inflight, k)) json_append(w2, json_copy(k));
    }
    /* awaitable = len(inflight - wedged - restart_safe) */
    size_t awaitable = 0;
    for (size_t i = 0; i < json_len(inflight); i++) {
        json_t *k = json_get(inflight, i);
        if (!sc2_arr_has_key(w2, k) && !sc2_arr_has_key(restart_safe, k)) awaitable++;
    }
    json_t *out = json_object();
    json_set(out, "awaitable", json_number((double)awaitable));
    json_set(out, "wedged", json_number((double)json_len(w2)));
    json_set(out, "restart_safe", json_number((double)json_len(restart_safe)));
    json_free(wedged); json_free(inflight); json_free(restart_safe); json_free(w2);
    return out;
}

/* PoP: sc2_record_external_cron_worker @ cron/scheduler.py:_record_external_cron_worker */
static json_t *sc2_record_external_cron_worker(sc2_io_t *io, json_t *in) {
    /* key = _inflight_key(job_id)  (home defaults None) */
    json_t *a = json_object();
    json_set_hdr(a, "job_id", json_get_str(in, "job_id", ""));
    json_set(a, "home", json_null());
    json_t *key = sc2_seam(io, "_inflight_key", a);
    if (sc2_raised(key)) return key;
    /* with suppress(TypeError, ValueError): _running_worker_pids[key] = int(pid) */
    long long ip = 0;
    json_t *err = NULL;
    if (sc2_py_int(json_obj_get(in, "pid"), &ip, &err)) {
        json_t *pids = sc2_st_ensure_arr(io, "_running_worker_pids");
        sc2_dm_set(pids, key, "pid", json_number((double)ip));
    } else if (err) {
        if (exc_is(err, "TypeError") || exc_is(err, "ValueError")) json_free(err);
        else { json_free(key); return err; }
    }
    /* if scope_isolated: _scope_isolated_job_ids.add(key) */
    if (jtruth(json_obj_get(in, "scope_isolated"))) {
        json_t *scope = sc2_st_ensure_arr(io, "_scope_isolated_job_ids");
        if (!sc2_arr_has_key(scope, key)) json_append(scope, json_copy(key));
    }
    json_free(key);
    return json_null();
}

/* PoP: sc2_cron_interval_minutes @ cron/scheduler.py:_cron_interval_minutes */
static json_t *sc2_cron_interval_minutes(sc2_io_t *io, json_t *in) {
    const char *expr = json_get_str(in, "expr", "");
    json_t *cache = sc2_st_ensure_obj(io, "_cron_interval_cache");
    if (json_has(cache, expr)) return json_copy(json_obj_get(cache, expr));
    json_t *result = json_null();
    bool ok = false;
    json_t *prop = NULL;
    do {
        if (sc2_import_fail(in, "cron.jobs")) break;  /* ImportError swallowed */
        json_t *r = sc2_seam(io, "_ensure_croniter", json_object());
        if (sc2_raised(r)) { prop = sc2_swallow_exception(r); break; }
        ok = jtruth(r);
        json_free(r);
        if (!ok) break;
        /* base = datetime.now() */
        json_t *base = sc2_seam(io, "datetime.now", json_object());
        if (sc2_raised(base)) { prop = sc2_swallow_exception(base); break; }
        /* it = _croniter(expr, base) */
        json_t *ca = json_object();
        json_set_hdr(ca, "expr", expr);
        json_set(ca, "base", base);  /* ownership transferred */
        json_t *it = sc2_seam(io, "croniter", ca);
        if (sc2_raised(it)) { prop = sc2_swallow_exception(it); break; }
        json_free(it);
        /* first = it.get_next(datetime); second = it.get_next(datetime) */
        json_t *f = sc2_seam(io, "croniter.get_next", json_object());
        if (sc2_raised(f)) { prop = sc2_swallow_exception(f); break; }
        json_t *s = sc2_seam(io, "croniter.get_next", json_object());
        if (sc2_raised(s)) { json_free(f); prop = sc2_swallow_exception(s); break; }
        sc2_dt d1 = sc2_iso_parse(jstr(f));
        sc2_dt d2 = sc2_iso_parse(jstr(s));
        json_free(f); json_free(s);
        if (d1.ok && d2.ok) {
            double gap = (sc2_dt_epoch(&d2) - sc2_dt_epoch(&d1)) / 60.0;
            if (gap > 0) { json_free(result); result = json_number(gap); }
        }
        /* a non-datetime result -> subtraction TypeError swallowed (result stays null) */
    } while (0);
    if (prop) { json_free(result); return prop; }
    /* cache a real cadence or a bad-expr None (ok True); skip transient import None */
    if (!j_is_null(result) || ok) json_set(cache, expr, json_copy(result));
    return result;
}

/* PoP: sc2_latest_executions_for_releasable_claims @ cron/scheduler.py:_latest_executions_for_releasable_claims */
static json_t *sc2_latest_executions_for_releasable_claims(sc2_io_t *io, json_t *in) {
    /* local_home = hermes_home_key(_get_hermes_home()) */
    json_t *hh = sc2_seam(io, "_get_hermes_home", json_object());
    if (sc2_raised(hh)) return hh;
    json_t *a = json_object();
    json_set(a, "home", hh);
    json_t *lh = sc2_seam(io, "hermes_home_key", a);
    if (sc2_raised(lh)) return lh;
    const char *local_home = jstr(lh);
    json_t *ids = sc2_st(io, "_running_job_ids");
    json_t *futs = sc2_st(io, "_running_futures");
    /* claim_futures = {key[1]: _running_futures.get(key) for key in _running_job_ids
     *                  if key[0] == local_home}  (insertion order; last wins) */
    json_t *cf = json_array();
    if (j_is_arr(ids)) {
        for (size_t i = 0; i < json_len(ids); i++) {
            json_t *k = json_get(ids, i);
            if (strcmp(sc2_key_home(k), local_home) != 0) continue;
            const char *jid = sc2_key_job(k);
            json_t *fv = sc2_dm_get(futs, k, "future");
            /* last-wins: replace existing job_id entry */
            json_t *existing = NULL;
            for (size_t j = 0; j < json_len(cf); j++) {
                json_t *e = json_get(cf, j);
                if (strcmp(jstr(json_obj_get(e, "job_id")), jid) == 0) { existing = e; break; }
            }
            if (existing) json_set(existing, "future", fv ? json_copy(fv) : json_null());
            else {
                json_t *e = json_object();
                json_set_hdr(e, "job_id", jid);
                json_set(e, "future", fv ? json_copy(fv) : json_null());
                json_append(cf, e);
            }
        }
    }
    /* candidates = [job_id for job_id,fut in claim_futures.items()
     *               if fut is None or fut is _FUTURE_PENDING or fut.done()] */
    json_t *candidates = json_array();
    for (size_t i = 0; i < json_len(cf); i++) {
        json_t *e = json_get(cf, i);
        const char *jid = jstr(json_obj_get(e, "job_id"));
        json_t *fut = json_obj_get(e, "future");
        bool cand = false;
        if (j_is_null(fut)) cand = true;
        else if (j_is_obj(fut) && jbool(json_obj_get(fut, "__pending__"))) cand = true;
        else {
            json_t *da = json_object();
            json_set_hdr(da, "id", jstr(json_obj_get(fut, "id")));
            json_t *dr = sc2_seam(io, "future.done", da);
            if (sc2_raised(dr)) { json_free(lh); json_free(cf); json_free(candidates); return dr; }
            cand = jtruth(dr);
            json_free(dr);
        }
        if (cand) json_append(candidates, json_string(jid));
    }
    json_free(cf);
    if (json_len(candidates) == 0) { json_free(lh); json_free(candidates); return json_object(); }
    /* try: from cron.executions import latest_executions; return latest_executions(candidates)
     * except Exception: return {} */
    json_t *out = json_object();
    if (!sc2_import_fail(in, "cron.executions")) {
        json_t *la = json_object();
        json_set(la, "job_ids", candidates);  /* ownership transferred */
        json_t *r = sc2_seam(io, "latest_executions", la);
        if (sc2_raised(r)) {
            json_t *p = sc2_swallow_exception(r);
            if (p) { json_free(lh); return p; }
        } else { json_free(out); out = r; }
    } else {
        json_free(candidates);
    }
    json_free(lh);
    return out;
}

/* PoP: sc2_row_belongs_to_claim @ cron/scheduler.py:_row_belongs_to_claim */
static json_t *sc2_row_belongs_to_claim(sc2_io_t *io, json_t *in) {
    json_t *row = json_obj_get(in, "row");
    json_t *ca = row ? json_obj_get(row, "claimed_at") : NULL;
    if (!jtruth(ca)) return json_bool(false);
    double claim_started = jnum(json_obj_get(in, "claim_started"));
    /* try: from cron.jobs import _ensure_aware; row_ts = _ensure_aware(fromisoformat(ca));
     *      return row_ts.timestamp() >= claim_started
     * except (ValueError, TypeError, OSError): return False */
    json_t *err = NULL;
    bool result = false;
    if (sc2_import_fail(in, "cron.jobs")) {
        /* ImportError is NOT in (ValueError,TypeError,OSError) -> propagates */
        return sc2_raise("ModuleNotFoundError", "import of cron.jobs halted; None in sys.modules");
    }
    if (!j_is_str(ca)) {
        err = sc2_raise("TypeError", "fromisoformat: argument must be str");
    } else {
        sc2_dt dt = sc2_iso_parse(ca->str_val);
        if (!dt.ok) {
            char *r = py_repr_str(ca->str_val);
            size_t cap = strlen(r) + 48;
            char *msg = malloc(cap);
            snprintf(msg, cap, "Invalid isoformat string: %s", r);
            err = sc2_raise("ValueError", msg);
            free(r); free(msg);
        } else {
            char *iso = sc2_dt_isoformat(&dt);
            json_t *aa = json_object();
            json_set_hdr(aa, "dt", iso);
            free(iso);
            json_t *aw = sc2_seam(io, "_ensure_aware", aa);
            if (sc2_raised(aw)) err = aw;
            else {
                sc2_dt ad = sc2_iso_parse(jstr(aw));
                json_free(aw);
                if (!ad.ok) {
                    err = sc2_raise("AttributeError", "'NoneType' object has no attribute 'timestamp'");
                } else {
                    result = sc2_dt_epoch(&ad) >= claim_started;
                }
            }
        }
    }
    if (err) {
        if (exc_is(err, "ValueError") || exc_is(err, "TypeError") || exc_is(err, "OSError")) {
            json_free(err);
            return json_bool(false);
        }
        return err;  /* other Exception / BaseException propagates */
    }
    return json_bool(result);
}
/* em-dash U+2014 (kept as a separate literal token so it never bleeds into a
 * following hex-digit character during implicit concatenation). */
#define SC2_MD "\xe2\x80\x94"

/* PoP: sc2_reclaim_fds_best_effort @ cron/scheduler.py:_reclaim_fds_best_effort */
static json_t *sc2_reclaim_fds_best_effort(sc2_io_t *io, json_t *in) {
    /* with suppress(Exception): import gc; gc.collect() */
    if (!sc2_import_fail(in, "gc")) {
        json_t *r = sc2_seam(io, "gc.collect", json_object());
        if (sc2_raised(r)) { json_t *p = sc2_swallow_exception(r); if (p) return p; }
        else json_free(r);
    }
    /* with suppress(Exception): from resource_limits import apply_nofile_soft_limit;
     * apply_nofile_soft_limit(None) */
    if (!sc2_import_fail(in, "hermes_cli.resource_limits")) {
        json_t *a = json_object();
        json_set(a, "limit", json_null());
        json_t *r = sc2_seam(io, "apply_nofile_soft_limit", a);
        if (sc2_raised(r)) { json_t *p = sc2_swallow_exception(r); if (p) return p; }
        else json_free(r);
    }
    return json_null();
}

/* PoP: sc2_load_prefill_messages @ cron/scheduler.py:_load_prefill_messages */
static json_t *sc2_load_prefill_messages(sc2_io_t *io, json_t *in) {
    json_t *cfg = json_obj_get(in, "cfg");
    const char *job_id = json_get_str(in, "job_id", "");
    /* agent_cfg = cfg.get("agent", {}) if isinstance(..., dict) else {} */
    json_t *agent_raw = cfg ? json_obj_get(cfg, "agent") : NULL;
    json_t *agent_cfg = j_is_obj(agent_raw) ? agent_raw : NULL;
    /* prefill_file = cron_env_setting(...) or cfg.get(...) or agent_cfg.get(...) */
    json_t *ea = json_object();
    json_set_hdr(ea, "name", "HERMES_PREFILL_MESSAGES_FILE");
    json_t *env = sc2_seam(io, "cron_env_setting", ea);
    if (sc2_raised(env)) return env;
    const char *prefill_file = jstr(env);   /* borrowed: keep env alive */
    if (!prefill_file[0]) {
        json_t *v = cfg ? json_obj_get(cfg, "prefill_messages_file") : NULL;
        if (jtruth(v)) prefill_file = jstr(v);
    }
    if (!prefill_file[0] && agent_cfg) {
        json_t *v = json_obj_get(agent_cfg, "prefill_messages_file");
        if (jtruth(v)) prefill_file = jstr(v);
    }
    if (!prefill_file[0]) { json_free(env); return json_null(); }
    /* pfpath = Path(prefill_file).expanduser() */
    json_t *xa = json_object();
    json_set_hdr(xa, "path", prefill_file);
    json_free(env);   /* prefill_file's last use is done (key was copied) */
    json_t *xp = sc2_seam(io, "path.expanduser", xa);
    if (sc2_raised(xp)) return xp;
    char *pfpath = strdup(jstr(xp));
    json_free(xp);
    /* if not pfpath.is_absolute(): pfpath = _get_hermes_home() / pfpath */
    if (pfpath[0] != '/') {
        json_t *hh = sc2_seam(io, "_get_hermes_home", json_object());
        if (sc2_raised(hh)) { free(pfpath); return hh; }
        char *joined = sc2_posix_join(jstr(hh), pfpath);
        json_free(hh);
        free(pfpath);
        pfpath = joined;
    }
    /* if not pfpath.exists(): return None */
    json_t *pa = json_object();
    json_set_hdr(pa, "path", pfpath);
    json_t *ex = sc2_seam(io, "path.exists", pa);
    if (sc2_raised(ex)) { free(pfpath); return ex; }
    bool exists = jtruth(ex);
    json_free(ex);
    if (!exists) { free(pfpath); return json_null(); }
    /* try: open+json.load; return list or None; except Exception: log + None */
    json_t *la = json_object();
    json_set_hdr(la, "path", pfpath);
    json_set_hdr(la, "encoding", "utf-8-sig");
    json_t *val = sc2_seam(io, "prefill.load", la);
    if (sc2_raised(val)) {
        /* snapshot str(e) BEFORE the swallow helper frees val (UAF otherwise) */
        json_t *ea = sc2_exc_arg(val);
        json_t *p = sc2_swallow_exception(val);
        if (p) { json_free(ea); free(pfpath); return p; }
        /* logger.warning(msg, job_id, pfpath, e) */
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_string(pfpath));
        json_append(args, ea);
        sc2_log(io, "warning",
                "Job '%s': failed to parse prefill messages file '%s': %s", args, 0);
        free(pfpath);
        return json_null();
    }
    free(pfpath);
    if (j_is_arr(val)) return val;
    json_free(val);
    return json_null();
}

/* PoP: sc2_blocked_config_result @ cron/scheduler.py:_blocked_config_result */
static json_t *sc2_blocked_config_result(sc2_io_t *io, json_t *in) {
    const char *job_id = json_get_str(in, "job_id", "");
    const char *job_name = json_get_str(in, "job_name", "");
    const char *reason = json_get_str(in, "_pf_reason", "");
    /* logger.warning(msg, job_name, job_id, _pf_reason) */
    {
        json_t *args = json_array();
        json_append(args, json_string(job_name));
        json_append(args, json_string(job_id));
        json_append(args, json_string(reason));
        sc2_log(io, "warning",
                "Job '%s' (ID: %s): BLOCKED by pre-dispatch config validation " SC2_MD
                " %s (no LLM call was made)", args, 0);
    }
    /* already_alerted = False; try: mark_preflight_alerted(job_id)
     * except Exception: logger.debug(..., exc_info=True) */
    bool already_alerted = false;
    if (sc2_import_fail(in, "cron.jobs")) {
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        sc2_log(io, "debug", "Job '%s': could not persist preflight alert marker", args, 1);
    } else {
        json_t *a = json_object();
        json_set_hdr(a, "job_id", job_id);
        json_t *r = sc2_seam(io, "mark_preflight_alerted", a);
        if (sc2_raised(r)) {
            json_t *p = sc2_swallow_exception(r);
            if (p) return p;
            json_t *args = json_array();
            json_append(args, json_string(job_id));
            sc2_log(io, "debug", "Job '%s': could not persist preflight alert marker", args, 1);
        } else {
            already_alerted = jtruth(r);
            json_free(r);
        }
    }
    const char *marker = already_alerted ? SC2_BLOCKED_CONFIG_SILENT_MARKER : SC2_BLOCKED_CONFIG_MARKER;
    /* _hermes_now().strftime('%Y-%m-%d %H:%M:%S')  (raise propagates) */
    json_t *nw = sc2_seam(io, "_hermes_now", json_object());
    if (sc2_raised(nw)) return nw;
    char *stamp = sc2_iso_strftime(jstr(nw));
    json_free(nw);
    sc2_sb b; sb_init(&b);
    sb_add(&b, "# Cron Job: "); sb_add(&b, job_name); sb_add(&b, "\n\n");
    sb_add(&b, "**Job ID:** "); sb_add(&b, job_id); sb_add(&b, "\n");
    sb_add(&b, "**Run Time:** "); sb_add(&b, stamp ? stamp : ""); sb_add(&b, "\n");
    sb_add(&b, "**Status:** BLOCKED (configuration)\n\n");
    sb_add(&b, "The pre-run configuration check found a problem, so the agent did not run "
               "(nothing was charged).\n\n");
    sb_add(&b, "**Reason:** "); sb_add(&b, reason); sb_add(&b, "\n\n");
    sb_add(&b, "Hermes tries again at the next scheduled time and clears this state on the first healthy "
               "run; this alert is not repeated. Check with `hermes cron doctor`. Set `cron.preflight: "
               "false` in config.yaml to disable this check.");
    free(stamp);
    char *doc = sb_take(&b);
    sc2_sb m; sb_init(&m);
    sb_add(&m, marker); sb_addch(&m, ' '); sb_add(&m, reason);
    char *errline = sb_take(&m);
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, json_string(doc));
    json_append(out, json_string(""));
    json_append(out, json_string(errline));
    free(doc); free(errline);
    return out;
}

/* PoP: sc2_preflight_or_block @ cron/scheduler.py:_preflight_or_block */
static json_t *sc2_preflight_or_block(sc2_io_t *io, json_t *in) {
    json_t *cfg = json_obj_get(in, "cfg");
    json_t *job = json_obj_get(in, "job");
    const char *job_id = json_get_str(in, "job_id", "");
    const char *job_name = json_get_str(in, "job_name", "");
    json_t *reason = json_null();   /* _pf_reason */
    json_t *prop = NULL;
    bool errored = false;
    do {
        json_t *a = json_object();
        json_set(a, "cfg", cfg ? json_copy(cfg) : json_null());
        json_t *en = sc2_seam(io, "_cron_preflight_enabled", a);
        if (sc2_raised(en)) { prop = sc2_swallow_exception(en); if (!prop) errored = true; break; }
        bool enabled = jtruth(en);
        json_free(en);
        if (!enabled) break;
        json_t *pa = json_object();
        json_set(pa, "job", job ? json_copy(job) : json_null());
        json_set(pa, "cfg", cfg ? json_copy(cfg) : json_null());
        json_t *pf = sc2_seam(io, "_preflight_job_config", pa);
        if (sc2_raised(pf)) { prop = sc2_swallow_exception(pf); if (!prop) errored = true; break; }
        json_free(reason);
        reason = pf;
        /* if not _pf_reason and job.get("preflight_alerted"): suppress clear */
        if (!jtruth(reason) && jtruth(job ? json_obj_get(job, "preflight_alerted") : NULL)) {
            if (!sc2_import_fail(in, "cron.jobs")) {
                json_t *ca = json_object();
                json_set_hdr(ca, "job_id", job_id);
                json_t *cr = sc2_seam(io, "clear_preflight_alerted", ca);
                if (sc2_raised(cr)) {
                    json_t *p = sc2_swallow_exception(cr);
                    if (p) { prop = p; break; }   /* BaseException passes the suppress */
                } else json_free(cr);
            }
        }
    } while (0);
    if (prop) { json_free(reason); return prop; }
    if (errored) {
        /* except Exception: logger.debug(..., exc_info=True); _pf_reason = None */
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        sc2_log(io, "debug", "Job '%s': preflight validation errored " SC2_MD " failing open", args, 1);
        json_free(reason);
        reason = json_null();
    }
    if (!jtruth(reason)) { json_free(reason); return json_null(); }
    /* return _blocked_config_result(job_id, job_name, _pf_reason) */
    json_t *sub = json_object();
    json_set_hdr(sub, "job_id", job_id);
    json_set_hdr(sub, "job_name", job_name);
    json_set(sub, "_pf_reason", reason);   /* ownership transferred */
    json_t *ifa = json_obj_get(in, "import_fail");
    if (ifa) json_set(sub, "import_fail", json_copy(ifa));
    json_t *r = sc2_blocked_config_result(io, sub);
    json_free(sub);
    return r;
}

/* PoP: sc2_load_credential_pool @ cron/scheduler.py:_load_credential_pool */
static json_t *sc2_load_credential_pool(sc2_io_t *io, json_t *in) {
    json_t *runtime = json_obj_get(in, "runtime");
    const char *job_id = json_get_str(in, "job_id", "");
    /* runtime_provider = str(runtime.get("provider") or "").strip().lower() */
    json_t *prov = runtime ? json_obj_get(runtime, "provider") : NULL;
    char *pstr = jtruth(prov) ? py_str(prov) : strdup("");
    /* strip ASCII whitespace then lower */
    size_t a = 0, b = strlen(pstr);
    while (a < b && (pstr[a]==' '||pstr[a]=='\t'||pstr[a]=='\n'||pstr[a]=='\r'||pstr[a]=='\v'||pstr[a]=='\f')) a++;
    while (b > a && (pstr[b-1]==' '||pstr[b-1]=='\t'||pstr[b-1]=='\n'||pstr[b-1]=='\r'||pstr[b-1]=='\v'||pstr[b-1]=='\f')) b--;
    char *provider = malloc(b - a + 1);
    for (size_t i = a; i < b; i++) { char c = pstr[i]; provider[i-a] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
    provider[b-a] = '\0';
    free(pstr);
    if (!provider[0]) { free(provider); return json_null(); }
    /* try: load_route_pool(...); if pool and has_credentials: log + return pool
     * except Exception as e: logger.debug(..., e); return None */
    json_t *out = json_null();
    if (sc2_import_fail(in, "agent.custom_entry_pool")) {
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_string(provider));
        json_append(args, json_string("import of agent.custom_entry_pool halted; None in sys.modules"));
        sc2_log(io, "debug", "Job '%s': failed to load credential pool for %s: %s", args, 0);
        free(provider);
        return out;
    }
    json_t *la = json_object();
    json_set_hdr(la, "provider", provider);
    json_set(la, "base_url", sc2_copy_or_null(runtime ? json_obj_get(runtime, "base_url") : NULL));
    json_set(la, "requested_provider", sc2_copy_or_null(runtime ? json_obj_get(runtime, "requested_provider") : NULL));
    json_t *pool = sc2_seam(io, "load_route_pool", la);
    if (sc2_raised(pool)) {
        /* snapshot str(e) BEFORE the swallow helper frees pool (UAF otherwise) */
        json_t *ea = sc2_exc_arg(pool);
        json_t *p = sc2_swallow_exception(pool);
        if (p) { json_free(ea); free(provider); return p; }
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_string(provider));
        json_append(args, ea);
        sc2_log(io, "debug", "Job '%s': failed to load credential pool for %s: %s", args, 0);
        free(provider);
        return out;
    }
    if (!j_is_null(pool) && jtruth(json_obj_get(pool, "has_credentials"))) {
        json_t *entries = json_obj_get(pool, "entries");
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_string(provider));
        json_append(args, json_number((double)json_len(entries)));
        sc2_log(io, "info", "Job '%s': loaded credential pool for provider %s with %d entries", args, 0);
        free(provider);
        return pool;   /* ownership transferred to ret */
    }
    json_free(pool);
    free(provider);
    return out;
}

/* PoP: sc2_init_cron_mcp_tools @ cron/scheduler.py:_init_cron_mcp_tools */
static json_t *sc2_init_cron_mcp_tools(sc2_io_t *io, json_t *in) {
    const char *job_id = json_get_str(in, "job_id", "");
    if (sc2_import_fail(in, "tools.mcp_tool_discovery")) {
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_string("import of tools.mcp_tool_discovery halted; None in sys.modules"));
        sc2_log(io, "warning", "Job '%s': MCP initialization failed (non-fatal): %s", args, 0);
        return json_null();
    }
    json_t *r = sc2_seam(io, "discover_mcp_tools", json_object());
    if (sc2_raised(r)) {
        /* snapshot str(e) BEFORE the swallow helper frees r (UAF otherwise) */
        json_t *ea = sc2_exc_arg(r);
        json_t *p = sc2_swallow_exception(r);
        if (p) { json_free(ea); return p; }
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, ea);
        sc2_log(io, "warning", "Job '%s': MCP initialization failed (non-fatal): %s", args, 0);
        return json_null();
    }
    if (jtruth(r)) {
        json_t *args = json_array();
        json_append(args, json_string(job_id));
        json_append(args, json_number((double)json_len(r)));
        sc2_log(io, "info", "Job '%s': %d MCP tool(s) available", args, 0);
    }
    json_free(r);
    return json_null();
}

/* PoP: sc2_raise_inactivity_timeout @ cron/scheduler.py:_raise_inactivity_timeout */
static json_t *sc2_raise_inactivity_timeout(sc2_io_t *io, json_t *in) {
    json_t *agent = json_obj_get(in, "agent");
    const char *job_name = json_get_str(in, "job_name", "");
    double limit_s = jnum(json_obj_get(in, "limit_s"));
    const char *agent_id = jstr(agent ? json_obj_get(agent, "id") : NULL);
    /* _activity = {}; if hasattr(agent,"get_activity_summary"): suppress call */
    json_t *activity = json_object();
    bool has_activity = jbool(agent ? json_obj_get(agent, "has_activity") : NULL);
    if (has_activity) {
        json_t *r = sc2_seam(io, "agent.get_activity_summary", json_object());
        if (sc2_raised(r)) {
            json_t *p = sc2_swallow_exception(r);
            if (p) { json_free(activity); return p; }
            /* swallowed: _activity stays {} */
        } else { json_free(activity); activity = r; }
    }
    /* _activity.get(...) — a non-dict _activity raises AttributeError (uncaught) */
    if (!j_is_obj(activity)) {
        const char *tn = py_type_name(activity);
        size_t cap = strlen(tn) + 64;
        char *msg = malloc(cap);
        snprintf(msg, cap, "'%s' object has no attribute 'get'", tn);
        json_t *e = sc2_raise("AttributeError", msg);
        free(msg); json_free(activity);
        return e;
    }
    json_t *last_desc_v = json_obj_get(activity, "last_activity_desc");
    const char *last_desc = last_desc_v ? jstr(last_desc_v) : "unknown";
    json_t *secs_v = json_obj_get(activity, "seconds_since_activity");
    double secs_ago = secs_v ? jnum(secs_v) : 0.0;
    json_t *api_v = json_obj_get(activity, "api_call_count");
    json_t *max_v = json_obj_get(activity, "max_iterations");
    json_t *tool_v = json_obj_get(activity, "current_tool");
    /* logger.error(msg, job_name, secs_ago, limit_s, last_desc, api, max, tool or "none") */
    {
        json_t *args = json_array();
        json_append(args, json_string(job_name));
        json_append(args, json_number(secs_ago));
        json_append(args, json_number(limit_s));
        json_append(args, json_string(last_desc));
        json_append(args, api_v ? json_copy(api_v) : json_number(0));
        json_append(args, max_v ? json_copy(max_v) : json_number(0));
        json_append(args, jtruth(tool_v) ? json_copy(tool_v) : json_string("none"));
        sc2_log(io, "error",
                "Job '%s' idle for %.0fs (inactivity limit %.0fs) "
                "| last_activity=%s | iteration=%s/%s | tool=%s", args, 0);
    }
    json_free(activity);
    /* request_hard_interrupt(agent, reason, tool_reason=...)  (raise propagates) */
    json_t *ha = json_object();
    json_set_hdr(ha, "agent", agent_id);
    json_set_hdr(ha, "reason", "Cron job timed out (inactivity)");
    json_set_hdr(ha, "tool_reason", "cron inactivity watchdog");
    json_t *hr = sc2_seam(io, "request_hard_interrupt", ha);
    if (sc2_raised(hr)) return hr;
    json_free(hr);
    /* raise TimeoutError(f"Cron job '{job_name}' idle for {int(secs)}s (limit {int(limit)}s)
     *                    — last activity: {last_desc}") */
    long long isecs = (long long)secs_ago;
    long long ilimit = (long long)limit_s;
    sc2_sb b; sb_init(&b);
    char t[64];
    sb_add(&b, "Cron job '"); sb_add(&b, job_name); sb_add(&b, "' idle for ");
    snprintf(t, sizeof t, "%lld", isecs); sb_add(&b, t);
    sb_add(&b, "s (limit ");
    snprintf(t, sizeof t, "%lld", ilimit); sb_add(&b, t);
    sb_add(&b, "s) " SC2_MD " last activity: ");
    sb_add(&b, last_desc);
    char *msg = sb_take(&b);
    json_t *e = sc2_raise("TimeoutError", msg);
    free(msg);
    return e;
}

/* PoP: sc2_run_doc_header @ cron/scheduler.py:_run_doc_header */
static json_t *sc2_run_doc_header(sc2_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    const char *title = json_get_str(in, "title", "");
    const char *job_id = json_get_str(in, "job_id", "");
    const char *prompt = json_get_str(in, "prompt", "");
    const char *prompt_stamp = json_get_str(in, "prompt_stamp", "");
    json_t *nw = sc2_seam(io, "_hermes_now", json_object());
    if (sc2_raised(nw)) return nw;
    char *stamp = sc2_iso_strftime(jstr(nw));
    json_free(nw);
    json_t *sd = job ? json_obj_get(job, "schedule_display") : NULL;
    char *sched = sd ? py_str(sd) : strdup("N/A");
    sc2_sb b; sb_init(&b);
    sb_add(&b, "# Cron Job: "); sb_add(&b, title); sb_add(&b, "\n\n");
    sb_add(&b, "**Job ID:** "); sb_add(&b, job_id); sb_add(&b, "\n");
    sb_add(&b, "**Run Time:** "); sb_add(&b, stamp ? stamp : ""); sb_add(&b, "\n");
    sb_add(&b, "**Schedule:** "); sb_add(&b, sched); sb_add(&b, "\n\n");
    sb_add(&b, prompt_stamp);
    sb_add(&b, SC2_PROMPT_HEADING);
    sb_add(&b, prompt);
    sb_add(&b, SC2_PROMPT_SEPARATOR);
    free(stamp); free(sched);
    char *doc = sb_take(&b);
    json_t *out = json_string(doc);
    free(doc);
    return out;
}
/* PoP: sc2_reload_dotenv_and_publish_delivery_target @ cron/scheduler.py:_reload_dotenv_and_publish_delivery_target */
static json_t *sc2_reload_dotenv_and_publish_delivery_target(sc2_io_t *io, json_t *in) {
    /* top-of-function imports (raise propagates; no try) */
    if (sc2_import_fail(in, "hermes_cli.env_loader"))
        return sc2_raise("ModuleNotFoundError", "import of hermes_cli.env_loader halted; None in sys.modules");
    if (sc2_import_fail(in, "gateway.session_context"))
        return sc2_raise("ModuleNotFoundError", "import of gateway.session_context halted; None in sys.modules");
    json_t *job = json_obj_get(in, "job");
    /* reset_secret_source_cache(_get_hermes_home()) */
    json_t *h1 = sc2_seam(io, "_get_hermes_home", json_object());
    if (sc2_raised(h1)) return h1;
    json_t *a1 = json_object();
    json_set(a1, "hermes_home", h1);
    json_t *r1 = sc2_seam(io, "reset_secret_source_cache", a1);
    if (sc2_raised(r1)) return r1;
    json_free(r1);
    /* load_hermes_dotenv(hermes_home=_get_hermes_home()) */
    json_t *h2 = sc2_seam(io, "_get_hermes_home", json_object());
    if (sc2_raised(h2)) return h2;
    json_t *a2 = json_object();
    json_set(a2, "hermes_home", h2);
    json_t *r2 = sc2_seam(io, "load_hermes_dotenv", a2);
    if (sc2_raised(r2)) return r2;
    json_free(r2);
    /* delivery_target = _resolve_delivery_target(job) */
    json_t *a3 = json_object();
    json_set(a3, "job", job ? json_copy(job) : json_null());
    json_t *target = sc2_seam(io, "_resolve_delivery_target", a3);
    if (sc2_raised(target)) return target;
    if (jtruth(target)) {
        /* _VAR_MAP[PLATFORM].set(target["platform"])  (KeyError if absent) */
        if (!json_has(target, "platform")) { json_free(target); return sc2_raise("KeyError", "'platform'"); }
        json_t *platform = json_obj_get(target, "platform");
        json_t *v1 = json_object();
        json_set_hdr(v1, "name", "HERMES_CRON_AUTO_DELIVER_PLATFORM");
        json_set(v1, "value", json_copy(platform));
        json_t *s1 = sc2_seam(io, "var_map.set", v1);
        if (sc2_raised(s1)) { json_free(target); return s1; }
        json_free(s1);
        /* _VAR_MAP[CHAT_ID].set(str(target["chat_id"])) */
        if (!json_has(target, "chat_id")) { json_free(target); return sc2_raise("KeyError", "'chat_id'"); }
        char *chat = py_str(json_obj_get(target, "chat_id"));
        json_t *v2 = json_object();
        json_set_hdr(v2, "name", "HERMES_CRON_AUTO_DELIVER_CHAT_ID");
        json_set_hdr(v2, "value", chat);
        free(chat);
        json_t *s2 = sc2_seam(io, "var_map.set", v2);
        if (sc2_raised(s2)) { json_free(target); return s2; }
        json_free(s2);
        /* _VAR_MAP[THREAD_ID].set("" if target.get("thread_id") is None else str(...)) */
        json_t *tv = json_obj_get(target, "thread_id");
        char *thread = j_is_null(tv) ? strdup("") : py_str(tv);
        json_t *v3 = json_object();
        json_set_hdr(v3, "name", "HERMES_CRON_AUTO_DELIVER_THREAD_ID");
        json_set_hdr(v3, "value", thread);
        free(thread);
        json_t *s3 = sc2_seam(io, "var_map.set", v3);
        if (sc2_raised(s3)) { json_free(target); return s3; }
        json_free(s3);
    }
    json_free(target);
    return json_null();
}

/* PoP: sc2_install_fire_secret_scope @ cron/scheduler.py:_install_fire_secret_scope */
static json_t *sc2_install_fire_secret_scope(sc2_io_t *io, json_t *in) {
    /* top imports in live order */
    static const char *mods[] = {"agent.secret_scope", "cron.scheduler_provider",
                                 "hermes_cli.env_loader", "tui_gateway.launch_profile_policy"};
    for (size_t i = 0; i < 4; i++) {
        if (sc2_import_fail(in, mods[i])) {
            char msg[128];
            snprintf(msg, sizeof msg, "import of %s halted; None in sys.modules", mods[i]);
            return sc2_raise("ModuleNotFoundError", msg);
        }
    }
    /* home = Path(_get_hermes_home()) */
    json_t *hh = sc2_seam(io, "_get_hermes_home", json_object());
    if (sc2_raised(hh)) return hh;
    const char *home = jstr(hh);
    /* hydrate_profile_secret_sources(home) */
    json_t *a1 = json_object();
    json_set_hdr(a1, "home", home);
    json_t *r1 = sc2_seam(io, "hydrate_profile_secret_sources", a1);
    if (sc2_raised(r1)) { json_free(hh); return r1; }
    json_free(r1);
    /* scope_token = set_secret_scope(owner_secret_scope(home), profile_home=str(home)) */
    json_t *a2 = json_object();
    json_set_hdr(a2, "home", home);
    json_t *scope = sc2_seam(io, "owner_secret_scope", a2);
    if (sc2_raised(scope)) { json_free(hh); return scope; }
    json_t *a3 = json_object();
    json_set(a3, "scope", scope);
    json_set_hdr(a3, "profile_home", home);
    json_t *scope_token = sc2_seam(io, "set_secret_scope", a3);
    if (sc2_raised(scope_token)) { json_free(hh); return scope_token; }
    /* context_token = set_multiplex_context(True) if routed_profile_fire() else None */
    json_t *rp = sc2_seam(io, "routed_profile_fire", json_object());
    if (sc2_raised(rp)) { json_free(hh); json_free(scope_token); return rp; }
    bool routed = jtruth(rp);
    json_free(rp);
    json_t *context_token = json_null();
    if (routed) {
        json_t *a4 = json_object();
        json_set(a4, "value", json_bool(true));
        json_t *ct = sc2_seam(io, "set_multiplex_context", a4);
        if (sc2_raised(ct)) { json_free(hh); json_free(scope_token); return ct; }
        json_free(context_token);
        context_token = ct;
    }
    json_free(hh);
    json_t *out = json_array();
    json_append(out, scope_token);
    json_append(out, context_token);
    return out;
}

/* PoP: sc2_reset_fire_secret_scope @ cron/scheduler.py:_reset_fire_secret_scope */
static json_t *sc2_reset_fire_secret_scope(sc2_io_t *io, json_t *in) {
    if (sc2_import_fail(in, "agent.secret_scope"))
        return sc2_raise("ModuleNotFoundError", "import of agent.secret_scope halted; None in sys.modules");
    /* scope_token, context_token = tokens  (2-unpack) */
    json_t *tokens = json_obj_get(in, "tokens");
    json_t *scope_token = NULL, *context_token = NULL;
    if (j_is_arr(tokens) && json_len(tokens) == 2) {
        scope_token = json_get(tokens, 0);
        context_token = json_get(tokens, 1);
    } else if (j_is_arr(tokens)) {
        size_t n = json_len(tokens);
        char msg[96];
        if (n < 2) snprintf(msg, sizeof msg, "not enough values to unpack (expected 2, got %zu)", n);
        else snprintf(msg, sizeof msg, "too many values to unpack (expected 2, got %zu)", n);
        return sc2_raise("ValueError", msg);
    } else {
        const char *tn = py_type_name(tokens);
        size_t cap = strlen(tn) + 64;
        char *msg = malloc(cap);
        snprintf(msg, cap, "cannot unpack non-iterable %s object", tn);
        json_t *e = sc2_raise("TypeError", msg);
        free(msg);
        return e;
    }
    /* if context_token is not None: reset_multiplex_context(context_token) */
    if (!j_is_null(context_token)) {
        json_t *a1 = json_object();
        json_set(a1, "token", json_copy(context_token));
        json_t *r1 = sc2_seam(io, "reset_multiplex_context", a1);
        if (sc2_raised(r1)) return r1;
        json_free(r1);
    }
    /* reset_secret_scope(scope_token) */
    json_t *a2 = json_object();
    json_set(a2, "token", json_copy(scope_token));
    json_t *r2 = sc2_seam(io, "reset_secret_scope", a2);
    if (sc2_raised(r2)) return r2;
    json_free(r2);
    return json_null();
}

/* PoP: sc2_start_owned_run @ cron/scheduler.py:_start_owned_run */
static json_t *sc2_start_owned_run(sc2_io_t *io, json_t *in) {
    json_t *job = json_obj_get(in, "job");
    const char *execution_id = json_get_str(in, "execution_id", "");
    /* if os.environ.get("_HERMES_CRON_EXTERNAL_WORKER") == execution_id: */
    json_t *ea = json_object();
    json_set_hdr(ea, "key", "_HERMES_CRON_EXTERNAL_WORKER");
    json_t *env = sc2_seam(io, "os.environ.get", ea);
    if (sc2_raised(env)) return env;
    bool external = j_is_str(env) && strcmp(env->str_val, execution_id) == 0;
    json_free(env);
    json_t *record = NULL;
    if (external) {
        json_t *a = json_object();
        json_set_hdr(a, "execution_id", execution_id);
        record = sc2_seam(io, "get_execution", a);
        if (sc2_raised(record)) return record;
    } else {
        json_t *a = json_object();
        json_set_hdr(a, "execution_id", execution_id);
        record = sc2_seam(io, "mark_execution_running", a);
        if (sc2_raised(record)) return record;
        if (j_is_null(record)) { json_free(record); return json_null(); }
    }
    /* return enter_cron_execution(job, execution_id, record or {}) */
    json_t *a = json_object();
    json_set(a, "job", job ? json_copy(job) : json_null());
    json_set_hdr(a, "execution_id", execution_id);
    json_set(a, "record", jtruth(record) ? json_copy(record) : json_object());
    json_free(record);
    json_t *r = sc2_seam(io, "enter_cron_execution", a);
    return r;
}

/* PoP: sc2_maybe_run_worktree_maintenance @ cron/scheduler.py:_maybe_run_worktree_maintenance */
static json_t *sc2_maybe_run_worktree_maintenance(sc2_io_t *io, json_t *in) {
    /* from hermes_constants import get_hermes_home  (raise propagates) */
    if (sc2_import_fail(in, "hermes_constants"))
        return sc2_raise("ModuleNotFoundError", "import of hermes_constants halted; None in sys.modules");
    json_t *hh = sc2_seam(io, "get_hermes_home", json_object());
    if (sc2_raised(hh)) return hh;
    const char *home = jstr(hh);   /* borrowed: keep hh alive until last use */
    /* now = time.monotonic() */
    json_t *nw = sc2_seam(io, "time.monotonic", json_object());
    if (sc2_raised(nw)) { json_free(hh); return nw; }
    double now = jnum(nw);
    json_free(nw);
    /* stamps = _last_worktree_maintenance_at or {}; last = stamps.get(home);
     * if last is not None and now-last < INTERVAL: return */
    json_t *stamps = sc2_st(io, "_last_worktree_maintenance_at");
    json_t *last = (j_is_obj(stamps)) ? json_obj_get(stamps, home) : NULL;
    if (!j_is_null(last) && (now - jnum(last)) < SC2_WORKTREE_MAINTENANCE_INTERVAL_SECONDS) {
        json_free(hh);
        return json_null();
    }
    /* _last_worktree_maintenance_at = {**stamps, home: now} */
    json_t *newstamps = j_is_obj(stamps) ? json_copy(stamps) : json_object();
    json_set(newstamps, home, json_number(now));
    json_free(hh);   /* home's last use is done (json_set copied the key) */
    json_set(io->state, "_last_worktree_maintenance_at", newstamps);
    /* threading.Thread(target=ctx.run, args=(_run,), name=..., daemon=True).start()
     * -> thread.start seam, then _run runs run_sweep (thread exceptions swallowed) */
    json_t *ta = json_object();
    json_set_hdr(ta, "name", "cron-worktree-prune");
    json_set(ta, "daemon", json_bool(true));
    json_t *ts = sc2_seam(io, "thread.start", ta);
    if (sc2_raised(ts)) return ts;
    json_free(ts);
    /* _run: from cron.scheduler_worktree_maintenance import run_sweep;
     * run_sweep(_worktree_maintenance_repos, interval_seconds=INTERVAL) */
    if (!sc2_import_fail(in, "cron.scheduler_worktree_maintenance")) {
        json_t *ra = json_object();
        json_set_hdr(ra, "repos", "_worktree_maintenance_repos");
        json_set(ra, "interval_seconds", json_number(SC2_WORKTREE_MAINTENANCE_INTERVAL_SECONDS));
        json_t *rs = sc2_seam(io, "run_sweep", ra);
        /* a raise inside the daemon thread is swallowed by threading (not observable) */
        if (rs) json_free(rs);
    }
    return json_null();
}
