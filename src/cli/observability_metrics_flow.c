/* observability_metrics_flow.c — FLOW + leaf port of the UNCLAIMED
 * hermes_cli/observability/shared_metrics_fields.py (the task-named
 * hermes_cli/observability/metrics.py does not exist in the quarry;
 * shared_metrics_fields.py is the real defining module of the bounded
 * shared-metrics field builders — all 20 live defs are unclaimed).
 *
 * The claimed siblings live in shared_metrics_contract / shared_metrics_catalog
 * ports and are SEAMED here through the injected universal seam om_io_t, as are
 * the unclaimed cross-module collaborators (contract._bucket / _norm /
 * count_bucket / size_bucket / execution_surface / model_error_class /
 * _non_negative_number, catalog.provider_metric_name / model_metric_name /
 * aux_task_metric_name / slash_command_metric_name / extension_metric_name,
 * signals.user_created_skill). Ported functions call each other REAL through
 * the *_core helpers (install_age_bucket -> _number, ttft_bucket -> _number,
 * context_fill_bucket -> _number, compression_fields -> context_fill_bucket +
 * compression_failure_class, model_token_fields -> provider_identifier,
 * extension_install_fields -> extension_failure_class + extension_registry,
 * extension_failure_class -> exception_failure_class -> tagged_failure_class,
 * milestones_for -> _long_session).
 *
 * Seam name table (name -> live call):
 *   _norm                          -> contract._norm(value)
 *   _bucket                        -> contract._bucket(value, thresholds, last)
 *   contract._non_negative_number  -> contract._non_negative_number(value)
 *   contract.count_bucket          -> contract.count_bucket(count)
 *   contract.size_bucket           -> contract.size_bucket(count)
 *   contract.execution_surface     -> contract.execution_surface(kwargs)
 *   contract.model_error_class     -> contract.model_error_class(kwargs)
 *   catalog.provider_metric_name   -> catalog.provider_metric_name(raw)
 *   catalog.model_metric_name      -> catalog.model_metric_name(raw, provider, max_length=)
 *   catalog.aux_task_metric_name   -> catalog.aux_task_metric_name(raw)
 *   catalog.slash_command_metric_name -> catalog.slash_command_metric_name(raw)
 *   catalog.extension_metric_name  -> catalog.extension_metric_name(kind, raw)
 *   user_created_skill             -> signals.user_created_skill(d)
 *
 * Representation contract (shared with tests/oracle_observability_metrics_flow.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded through
 *     every helper; an uncaught one becomes the case ret. The module has NO
 *     except guards, so every raise (incl. the BaseException trio SystemExit /
 *     KeyboardInterrupt / GeneratorExit) propagates straight to the case ret.
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError raise
 *     spec ("no script for <name>"), recorded as the event ret BEFORE propagation.
 *   - scripts: a bare JSON array is a per-call QUEUE (one result per call); a
 *     list RESULT must be wrapped {"__script__": true, "ret": [...]}. A scripted
 *     null returns None (NULL pointer). None-valued seam-arg OBJECT keys dropped
 *     on both sides.
 *   - exception doubles {"__exc__": true, "type": name, "module": mod,
 *     "mro": [{"name","module"}...], "failure_class": str?, "message": str?,
 *     "hook": bool?, "failure_class_property": bool?} model BaseException
 *     instances. Builtin type names resolve through the FULL builtin exception
 *     parent table below (CPython 3.14 hierarchy, incl. ExceptionGroup's two
 *     bases); non-builtin types carry their explicit mro. Attribute doubles
 *     (plain JSON objects without __exc__) model SimpleNamespace (getattr
 *     .value / static failure_class reads). inspect.getattr_static is modelled
 *     by the direct static "failure_class" field read (never runs hooks).
 *   - tuple/set -> array; None -> null; integral floats collapse to ints on
 *     both sides.
 *   - fixture domain (ASCII scope): str operands ASCII; _norm seam results are
 *     str | null | raise; _non_negative_number seam results are number | null |
 *     raise; usage token amounts are INTEGER literals (int identity) or
 *     non-integral floats / bools / strs / nulls — integral-FLOAT literals are
 *     EXCLUDED everywhere (libjson has no int/float identity, so 5.0 reads as 5
 *     and isinstance(value, int) would diverge from live). Floats reaching seam
 *     args/rets are integral or binary-exact short decimals (0.5 / 0.75 / 1.25 /
 *     12.5 ...) so serialization round-trips exactly. Error inputs are exc
 *     doubles, attribute doubles, null or str (no numbers).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/observability_metrics_flow.c".
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── universal seam ── */
typedef struct om_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} om_io_t;

/* ── truthiness / accessors (Python-faithful) ── */
static bool om_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool om_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool om_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool om_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool om_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool om_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool om_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *om_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* json_obj_get PRESENCE semantics: NULL == absent; a json_null node == present
 * null. Non-object carriers read as absent (callers gate .get separately). */
static json_t *om_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double om_num(const json_t *o, const char *k, double def)
{
    const json_t *v = om_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool om_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool om_num_integral(double d)
{
    return d == (double)(long long)d && fabs(d) < 9.2e18;
}

/* ── raise specs ── */
static bool om_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *om_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *om_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *om_call(om_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = om_xfmt("no seam installed for %s", name);
        json_t *r = om_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void om_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void om_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}

/* ── static data transcribed from the quarry (shared_metrics_fields.py +
 * shared_metrics_contract.py closed enums + tools/skills_hub_install.py
 * _SOURCE_ID_ALIASES). The oracle seeds the LIVE values; the shadow diff
 * verifies these tables against them. ── */

/* fields._SESSION_DURATION_THRESHOLDS / _INSTALL_AGE_THRESHOLDS /
 * _TTFT_THRESHOLDS / _CONTEXT_FILL_THRESHOLDS (upper, label) */
typedef struct { double upper; const char *label; } om_threshold_t;
static const om_threshold_t OM_SESSION_DURATION_THRESHOLDS[] = {
    {60000, "lt_1m"}, {300000, "1m_to_5m"}, {1800000, "5m_to_30m"},
    {7200000, "30m_to_2h"}, {28800000, "2h_to_8h"},
};
static const om_threshold_t OM_INSTALL_AGE_THRESHOLDS[] = {
    {3600, "lt_1h"}, {86400, "1h_to_1d"}, {604800, "1d_to_7d"},
    {2592000, "7d_to_30d"}, {7776000, "30d_to_90d"},
};
static const om_threshold_t OM_TTFT_THRESHOLDS[] = {
    {0.5, "lt_500ms"}, {1.0, "500ms_to_1s"}, {2.0, "1s_to_2s"},
    {5.0, "2s_to_5s"}, {15.0, "5s_to_15s"},
};
static const om_threshold_t OM_CONTEXT_FILL_THRESHOLDS[] = {
    {50, "lt_50"}, {75, "50_to_75"}, {90, "75_to_90"}, {100, "90_to_100"},
};
#define OM_N(a) (sizeof(a) / sizeof((a)[0]))

/* fields._USAGE_KEYS (insertion order is semantic) */
static const struct { const char *token_type; const char *key; } OM_USAGE_KEYS[] = {
    {"input", "input_tokens"}, {"output", "output_tokens"},
    {"cache_read", "cache_read_tokens"}, {"cache_write", "cache_write_tokens"},
    {"reasoning", "reasoning_tokens"},
};

/* contract closed enums read directly by the field builders */
static const char *OM_TASK_OUTCOMES[] = {
    "cancelled", "failed", "success", "timed_out", "unknown",
};
static const char *OM_SETUP_SURFACES[] = { "cli", "desktop", "other" };
static const char *OM_EXTENSION_KINDS[] = { "mcp_server", "plugin", "skill" };
static const char *OM_EXTENSION_SOURCES[] = {
    "bundled", "catalog", "hub", "local", "other", "url",
};
/* contract.COMPRESSION_SKIP_CLASSES | frozenset({...}) */
static const char *OM_COMPRESSION_FAILURE_CLASSES[] = {
    "attempt_superseded", "empty_post_handoff_window", "explicit_interrupt",
    "insufficient_messages", "lock_contended", "no_compressible_window", "snapshot_stale",
    "aux_model_fallback", "commit_fence_cancelled", "empty_transcript", "exception",
    "feasibility_skip", "no_progress", "none", "other", "pool_saturated", "rollback",
    "session_split_failed", "stall_deterministic_fallback", "stall_interrupted",
    "summary_auth_failure", "summary_empty_content_failure", "summary_generation_aborted",
    "summary_generation_failed", "summary_network_failure", "summary_overload_degraded",
    "summary_overload_failure", "summary_truncated_failure", "unknown", "would_grow",
};
/* contract.EXTENSION_COMMON_FAILURE_CLASSES | per-kind extras */
static const char *OM_EXT_COMMON_FAILURE_CLASSES[] = {
    "exception", "filesystem_error", "network", "none", "other", "permission",
};
static const char *OM_EXT_SKILL_EXTRA[] = {
    "ambiguous", "auth_rejected", "fetch_failed", "invalid_bundle", "invalid_name",
    "not_found", "rate_limited", "scan_blocked", "stale_index",
};
static const char *OM_EXT_PLUGIN_EXTRA[] = {
    "already_installed", "clone_failed", "deps_declined", "deps_failed", "git_missing",
    "incompatible", "invalid_source", "manifest_invalid", "non_interactive",
    "removed_from_catalog", "scan_blocked",
};
static const char *OM_EXT_MCP_EXTRA[] = {
    "auth_required", "bootstrap_failed", "clone_failed", "config_invalid",
    "config_rejected", "connect_failed", "git_missing", "missing_credentials",
    "server_start_failed",
};
/* contract.EXTENSION_REGISTRY_IDS | {"none","other","unresolved"} */
static const char *OM_EXTENSION_REGISTRIES[] = {
    "browse-sh", "clawhub", "github", "hermes-index", "lobehub", "official",
    "skills-sh", "url", "well-known", "none", "other", "unresolved",
};
/* tools/skills_hub_install.py:_SOURCE_ID_ALIASES */
static const struct { const char *from; const char *to; } OM_SOURCE_ID_ALIASES[] = {
    {"skills.sh", "skills-sh"},
};
/* contract.MODEL_IDENTIFIER_MAX_LENGTH */
#define OM_MODEL_IDENTIFIER_MAX_LENGTH 256

/* fields._COMPRESSION_TRIGGERS (coarse buckets) */
static const struct { const char *key; const char *bucket; } OM_COMPRESSION_TRIGGERS[] = {
    {"auto", "auto"}, {"threshold", "auto"}, {"preflight", "auto"}, {"auto_threshold", "auto"},
    {"idle", "auto"}, {"turn_start_threshold", "auto"}, {"engine_preflight", "auto"},
    {"pre_api", "auto"}, {"post_tool", "auto"}, {"gateway_hygiene", "auto"}, {"cache_expiry", "auto"},
    {"overflow", "overflow"}, {"context_overflow", "overflow"}, {"overflow_error", "overflow"},
    {"error", "overflow"},
    {"manual", "manual"}, {"user", "manual"}, {"command", "manual"}, {"slash", "manual"},
};
/* fields._COMPRESSION_OUTCOMES */
static const struct { const char *key; const char *bucket; } OM_COMPRESSION_OUTCOMES[] = {
    {"success", "success"}, {"ok", "success"}, {"compressed", "success"}, {"completed", "success"},
    {"skipped", "skipped"}, {"noop", "skipped"}, {"not_needed", "skipped"},
};
/* fields._COMPRESSION_FAILURE_CLASS_ALIASES */
static const struct { const char *from; const char *to; } OM_COMPRESSION_FAILURE_CLASS_ALIASES[] = {
    {"summary_model_benched", "feasibility_skip"},
};
/* fields._NETWORK_MODULES (top-level package names) */
static const char *OM_NETWORK_MODULES[] = {
    "aiohttp", "h11", "h2", "httpcore", "httpx", "requests", "ssl", "urllib3", "websockets",
};
/* contract metric-name constants keying fields._MILESTONE_RULES */
#define OM_TASK_STARTED_METRIC      "hermes.task_run.started"
#define OM_TASK_FINISHED_METRIC     "hermes.task_run.finished"
#define OM_TOOL_USAGE_METRIC        "hermes.tool.usage.count"
#define OM_SKILL_LIFECYCLE_METRIC   "hermes.skill.lifecycle.count"
#define OM_SKILL_LOAD_METRIC        "hermes.skill.load.count"
#define OM_SESSION_METRIC           "hermes.session.count"
#define OM_SETUP_COMPLETED_METRIC   "hermes.setup.completed"

/* ── FULL builtin exception parent table (CPython 3.14.9, probed from the
 * quarry venv: every builtins BaseException subclass). ExceptionGroup has TWO
 * bases (BaseExceptionGroup, Exception) and is special-cased in om_exc_mro. ── */
typedef struct { const char *name; const char *parent; } om_exc_parent_t;
static const om_exc_parent_t OM_EXC_PARENTS[] = {
    {"BaseException", "object"},
    {"object", NULL},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"BufferError", "Exception"},
    {"BytesWarning", "Warning"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"},
    {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"},
    {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"},
    {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"},
    {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"},
    {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"},
    {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"},
    {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"},
    {"SystemError", "Exception"},
    {"SystemExit", "BaseException"},
    {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"},
    {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"_IncompleteInputError", "SyntaxError"},
};

static const char *om_exc_parent(const char *name)
{
    for (size_t i = 0; i < OM_N(OM_EXC_PARENTS); i++)
        if (om_str_eq(OM_EXC_PARENTS[i].name, name)) return OM_EXC_PARENTS[i].parent;
    return NULL;
}
static bool om_exc_builtin(const char *name)
{
    if (om_str_eq(name, "object")) return false;
    for (size_t i = 0; i < OM_N(OM_EXC_PARENTS); i++)
        if (om_str_eq(OM_EXC_PARENTS[i].name, name)) return true;
    return false;
}

/* type(error).__name__ for the scalar / attribute-double fallbacks */
static const char *om_type_name(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) return om_num_integral(v->num_val) ? "int" : "float";
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (v->type == JSON_OBJECT) {
        if (om_truth(om_get(v, "__exc__"))) {
            const char *t = om_str(om_get(v, "type"));
            return t ? t : "Exception";
        }
        return "SimpleNamespace";
    }
    return "object";
}

/* type(error).__mro__ as (name, module) pairs. Builtin exc types walk the
 * parent table; non-builtin exc doubles carry their explicit mro; attribute
 * doubles are SimpleNamespace; scalars use their one-step chains. */
#define OM_MRO_MAX 16
static int om_exc_mro(const json_t *err, const char *names[], const char *mods[])
{
    int n = 0;
    if (om_is_obj(err) && om_truth(om_get(err, "__exc__"))) {
        const char *t = om_str(om_get(err, "type"));
        if (!t) t = "Exception";
        if (om_str_eq(t, "ExceptionGroup")) {
            names[0] = "ExceptionGroup"; names[1] = "BaseExceptionGroup";
            names[2] = "Exception"; names[3] = "BaseException"; names[4] = "object";
            for (int i = 0; i < 5; i++) mods[i] = "builtins";
            return 5;
        }
        if (om_exc_builtin(t)) {
            const char *cur = t;
            while (cur && n < OM_MRO_MAX) {
                names[n] = cur; mods[n] = "builtins"; n++;
                cur = om_exc_parent(cur);
            }
            return n;
        }
        const json_t *mro = om_get(err, "mro");
        if (om_is_arr(mro)) {
            for (size_t i = 0; i < mro->c.count && n < OM_MRO_MAX; i++) {
                const json_t *e = mro->c.items[i];
                const char *nm = om_str(om_get(e, "name"));
                const char *md = om_str(om_get(e, "module"));
                names[n] = nm ? nm : "object";
                mods[n] = md ? md : "builtins";
                n++;
            }
            if (n > 0) return n;
        }
        const char *md = om_str(om_get(err, "module"));
        names[0] = t; mods[0] = md ? md : "builtins";
        names[1] = "object"; mods[1] = "builtins";
        return 2;
    }
    if (om_is_obj(err)) { names[0] = "SimpleNamespace"; mods[0] = "types"; }
    else if (om_is_null(err)) { names[0] = "NoneType"; mods[0] = "builtins"; }
    else if (om_is_str(err)) { names[0] = "str"; mods[0] = "builtins"; }
    else if (om_is_bool(err)) { names[0] = "bool"; mods[0] = "builtins"; }
    else if (om_is_arr(err)) { names[0] = "list"; mods[0] = "builtins"; }
    else if (om_is_num(err)) { names[0] = om_num_integral(err->num_val) ? "int" : "float"; mods[0] = "builtins"; }
    else { names[0] = "object"; mods[0] = "builtins"; }
    names[1] = "object"; mods[1] = "builtins";
    return 2;
}

/* ── string-set membership + py3.14 unhashable TypeError ── */
static bool om_str_in(const json_t *v, const char *const *set, size_t n)
{
    const char *s = om_str(v);
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (om_str_eq(s, set[i])) return true;
    return false;
}
/* `x in <set>` raises for unhashable carriers (py3.14 message). Returns a raise
 * spec or NULL. */
static json_t *om_set_hashable(const json_t *v)
{
    if (om_is_obj(v)) return om_mkraise("TypeError",
        "cannot use 'dict' as a set element (unhashable type: 'dict')");
    if (om_is_arr(v)) return om_mkraise("TypeError",
        "cannot use 'list' as a set element (unhashable type: 'list')");
    return NULL;
}
/* `<carrier>.get` on a non-dict raises AttributeError BEFORE any further work.
 * Returns a raise spec or NULL. */
static json_t *om_get_carrier(const json_t *v)
{
    if (!v || v->type == JSON_OBJECT) return NULL;
    char *m = om_xfmt("'%s' object has no attribute 'get'", om_type_name(v));
    json_t *r = om_mkraise("AttributeError", m ? m : "'?' object has no attribute 'get'");
    free(m);
    return r;
}
/* str.split(":", 1)[0] — malloc'd prefix (whole string when no colon). */
static char *om_split_colon(const char *s)
{
    const char *c = strchr(s ? s : "", ':');
    size_t n = c ? (size_t)(c - s) : strlen(s ? s : "");
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s ? s : "", n);
    r[n] = '\0';
    return r;
}
/* `_norm(...).split` on a non-str seam result: AttributeError with the live
 * type name (null -> NoneType). Returns a raise spec or NULL. */
static json_t *om_split_carrier(const json_t *v)
{
    if (om_is_str(v)) return NULL;
    char *m = om_xfmt("'%s' object has no attribute 'split'", om_type_name(v));
    json_t *r = om_mkraise("AttributeError", m ? m : "'?' object has no attribute 'split'");
    free(m);
    return r;
}

/* ── seam wrappers (one per live collaborator call; args match the oracle
 * proj() shape exactly) ── */
static json_t *om_norm(om_io_t *io, const json_t *value)
{
    json_t *a = json_object();
    om_arg_j(a, "value", value);
    return om_call(io, "_norm", a);
}
static json_t *om_nn(om_io_t *io, const json_t *value)
{
    json_t *a = json_object();
    om_arg_j(a, "value", value);
    return om_call(io, "contract._non_negative_number", a);
}
static json_t *om_bucket(om_io_t *io, double value,
                         const om_threshold_t *th, size_t n, const char *last)
{
    json_t *a = json_object();
    json_set(a, "value", json_number(value));
    json_t *t = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_number(th[i].upper));
        json_append(pair, json_string(th[i].label));
        json_append(t, pair);
    }
    json_set(a, "thresholds", t);
    om_arg_s(a, "last", last);
    return om_call(io, "_bucket", a);
}
static json_t *om_count_bucket(om_io_t *io, double count)
{
    json_t *a = json_object();
    json_set(a, "count", json_number(count));
    return om_call(io, "contract.count_bucket", a);
}
static json_t *om_size_bucket(om_io_t *io, double count)
{
    json_t *a = json_object();
    json_set(a, "count", json_number(count));
    return om_call(io, "contract.size_bucket", a);
}
static json_t *om_exec_surface(om_io_t *io, const json_t *surface)
{
    json_t *kw = json_object();
    om_arg_j(kw, "execution_surface", surface);
    json_t *a = json_object();
    json_set(a, "kwargs", kw);
    return om_call(io, "contract.execution_surface", a);
}
static json_t *om_model_error_class(om_io_t *io, const json_t *reason)
{
    json_t *kw = json_object();
    om_arg_j(kw, "reason", reason);
    json_t *a = json_object();
    json_set(a, "kwargs", kw);
    return om_call(io, "contract.model_error_class", a);
}
static json_t *om_provider_name(om_io_t *io, const json_t *raw)
{
    json_t *a = json_object();
    om_arg_j(a, "raw", raw);
    return om_call(io, "catalog.provider_metric_name", a);
}
static json_t *om_model_name(om_io_t *io, const json_t *raw, const json_t *provider)
{
    json_t *a = json_object();
    om_arg_j(a, "raw", raw);
    om_arg_j(a, "provider", provider);
    json_set(a, "max_length", json_number(OM_MODEL_IDENTIFIER_MAX_LENGTH));
    return om_call(io, "catalog.model_metric_name", a);
}
static json_t *om_aux_name(om_io_t *io, const json_t *raw)
{
    json_t *a = json_object();
    om_arg_j(a, "raw", raw);
    return om_call(io, "catalog.aux_task_metric_name", a);
}
static json_t *om_slash_name(om_io_t *io, const json_t *raw)
{
    json_t *a = json_object();
    om_arg_j(a, "raw", raw);
    return om_call(io, "catalog.slash_command_metric_name", a);
}
static json_t *om_ext_name(om_io_t *io, const char *kind, const json_t *raw)
{
    json_t *a = json_object();
    om_arg_s(a, "kind", kind);
    om_arg_j(a, "raw", raw);
    return om_call(io, "catalog.extension_metric_name", a);
}
static json_t *om_user_created_skill(om_io_t *io, const json_t *d)
{
    json_t *a = json_object();
    om_arg_j(a, "d", d);
    return om_call(io, "user_created_skill", a);
}

/* ── exception classification cores (no seams: pure reads of the double) ── */

/* tagged_failure_class: inspect.getattr_static(error, "failure_class", None);
 * static field read only — never runs __getattr__ / property hooks. */
static json_t *om_tagged_core(const json_t *error)
{
    const json_t *tagged = om_get(error, "failure_class");
    if (om_is_str(tagged) && tagged->str_val && tagged->str_val[0])
        return json_copy(tagged);
    return NULL;
}

/* exception_failure_class: tagged -> permission -> network -> filesystem_error
 * -> exception:<Type>. isinstance gates scan the FULL mro; the network gate
 * also scans every mro class's top-level __module__ against _NETWORK_MODULES. */
static json_t *om_exception_failure_class_core(const json_t *error)
{
    json_t *tagged = om_tagged_core(error);
    if (tagged) return tagged;
    const char *names[OM_MRO_MAX]; const char *mods[OM_MRO_MAX];
    int n = om_exc_mro(error, names, mods);
    for (int i = 0; i < n; i++)
        if (om_str_eq(names[i], "PermissionError")) return json_string("permission");
    for (int i = 0; i < n; i++) {
        if (om_str_eq(names[i], "ConnectionError") || om_str_eq(names[i], "TimeoutError"))
            return json_string("network");
        if (om_str_eq(names[i], "gaierror") && om_str_eq(mods[i], "socket"))
            return json_string("network");
        if (om_str_eq(names[i], "URLError") && om_str_eq(mods[i], "urllib.error"))
            return json_string("network");
        const char *md = mods[i] ? mods[i] : "";
        const char *dot = strchr(md, '.');
        size_t rootlen = dot ? (size_t)(dot - md) : strlen(md);
        for (size_t k = 0; k < OM_N(OM_NETWORK_MODULES); k++)
            if (strlen(OM_NETWORK_MODULES[k]) == rootlen
                && strncmp(md, OM_NETWORK_MODULES[k], rootlen) == 0)
                return json_string("network");
    }
    for (int i = 0; i < n; i++)
        if (om_str_eq(names[i], "OSError")) return json_string("filesystem_error");
    char *m = om_xfmt("exception:%s", names[0]);
    json_t *r = json_string(m ? m : "exception:?");
    free(m);
    return r;
}

/* ── ported leaves / flows ── */

static json_t *om_number_core(om_io_t *io, const json_t *value)
{
    return om_nn(io, value);
}

/* PoP: om_number @ hermes_cli/observability/shared_metrics_fields.py:_number */
static json_t *om_number(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return om_number_core(io, om_get(in, "value"));
}

static json_t *om_provider_identifier_core(om_io_t *io, const json_t *value)
{
    return om_provider_name(io, value);
}

/* PoP: om_provider_identifier @ hermes_cli/observability/shared_metrics_fields.py:provider_identifier */
static json_t *om_provider_identifier(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return om_provider_identifier_core(io, om_get(in, "value"));
}

/* PoP: om_session_fields @ hermes_cli/observability/shared_metrics_fields.py:session_fields */
static json_t *om_session_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *sf = om_get(in, "start_fields");   /* absent -> {} */
    double active_ms = om_num(in, "active_ms", 0);
    double mx = active_ms < 0 ? 0 : active_ms;
    json_t *ab = om_bucket(io, mx, OM_SESSION_DURATION_THRESHOLDS,
                           OM_N(OM_SESSION_DURATION_THRESHOLDS), "gte_8h");
    if (om_is_raise(ab)) return ab;
    /* entrypoint .get fires AFTER the _bucket seam (dict value order). */
    json_t *bad = om_get_carrier(sf);
    if (bad) { json_free(ab); return bad; }
    const json_t *ep = om_get(sf, "entrypoint");
    const json_t *es = om_get(sf, "execution_surface");
    json_t *ct = om_count_bucket(io, om_num(in, "failed_turns", 0));
    if (om_is_raise(ct)) { json_free(ab); return ct; }
    const json_t *lo = om_get(in, "last_outcome");
    json_t *uh = om_set_hashable(lo);
    if (uh) { json_free(ab); json_free(ct); return uh; }
    const json_t *pf = om_get(sf, "platform");
    json_t *tb = om_size_bucket(io, om_num(in, "turns", 0));
    if (om_is_raise(tb)) { json_free(ab); json_free(ct); return tb; }
    json_t *o = json_object();
    json_set(o, "active_duration_bucket", ab);
    json_set(o, "entrypoint", ep ? json_copy(ep) : json_string("unknown"));
    json_set(o, "execution_surface", es ? json_copy(es) : json_string("unknown"));
    json_set(o, "failed_turn_count_bucket", ct);
    json_set(o, "last_outcome",
             om_str_in(lo, OM_TASK_OUTCOMES, OM_N(OM_TASK_OUTCOMES))
                 ? json_copy(lo) : json_string("unknown"));
    json_set(o, "platform", pf ? json_copy(pf) : json_string("none"));
    json_set(o, "turn_count_bucket", tb);
    return o;
}

/* PoP: om_install_age_bucket @ hermes_cli/observability/shared_metrics_fields.py:install_age_bucket */
static json_t *om_install_age_bucket(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *age = om_number_core(io, om_get(in, "age_seconds"));
    if (om_is_raise(age)) return age;
    if (!age) return json_string("unknown");
    double a = om_is_num(age) ? age->num_val : 0;
    json_free(age);
    return om_bucket(io, a, OM_INSTALL_AGE_THRESHOLDS,
                     OM_N(OM_INSTALL_AGE_THRESHOLDS), "gte_90d");
}

/* PoP: om_ttft_bucket @ hermes_cli/observability/shared_metrics_fields.py:ttft_bucket */
static json_t *om_ttft_bucket(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *ev = om_get(in, "event");          /* absent -> {} */
    json_t *bad = om_get_carrier(ev);
    if (bad) return bad;
    /* both _number seams fire before either None gate (tuple assignment). */
    json_t *started = om_nn(io, om_get(ev, "started_at"));
    if (om_is_raise(started)) return started;
    json_t *first = om_nn(io, om_get(ev, "first_chunk_at"));
    if (om_is_raise(first)) { json_free(started); return first; }
    if (!started) { json_free(first); return json_string("unknown"); }
    if (!first) { json_free(started); return json_string("not_streamed"); }
    double s = om_is_num(started) ? started->num_val : 0;
    double f = om_is_num(first) ? first->num_val : 0;
    json_free(started); json_free(first);
    double diff = f - s;
    double mx = diff < 0 ? 0.0 : diff;
    return om_bucket(io, mx, OM_TTFT_THRESHOLDS, OM_N(OM_TTFT_THRESHOLDS), "gte_15s");
}

static json_t *om_context_fill_core(om_io_t *io, const json_t *tokens_raw,
                                    const json_t *length_raw)
{
    json_t *tokens = om_nn(io, tokens_raw);
    if (om_is_raise(tokens)) return tokens;
    json_t *length = om_nn(io, length_raw);
    if (om_is_raise(length)) { json_free(tokens); return length; }
    if (!tokens) { json_free(length); return json_string("unknown"); }
    /* `not length`: None or 0 (incl. 0.0) -> unknown; negatives proceed. */
    if (!length || (om_is_num(length) && length->num_val == 0.0)) {
        json_free(tokens); json_free(length);
        return json_string("unknown");
    }
    double t = om_is_num(tokens) ? tokens->num_val : 0;
    double l = om_is_num(length) ? length->num_val : 0;
    json_free(tokens); json_free(length);
    return om_bucket(io, 100.0 * t / l, OM_CONTEXT_FILL_THRESHOLDS,
                     OM_N(OM_CONTEXT_FILL_THRESHOLDS), "gte_100");
}

/* PoP: om_context_fill_bucket @ hermes_cli/observability/shared_metrics_fields.py:context_fill_bucket */
static json_t *om_context_fill_bucket(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return om_context_fill_core(io, om_get(in, "tokens_before"), om_get(in, "context_length"));
}

/* PoP: om_model_token_fields @ hermes_cli/observability/shared_metrics_fields.py:model_token_fields */
static json_t *om_model_token_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *usage = om_get(in, "usage");
    if (!om_is_obj(usage)) return NULL;
    long long amounts[OM_N(OM_USAGE_KEYS)];
    bool any = false;
    for (size_t i = 0; i < OM_N(OM_USAGE_KEYS); i++) {
        const json_t *v = om_get(usage, OM_USAGE_KEYS[i].key);
        /* isinstance(value, int) and not bool and value > 0: JSON integer
         * literals only (integral floats excluded from the fixture domain). */
        if (om_is_num(v) && om_num_integral(v->num_val) && v->num_val > 0) {
            amounts[i] = (long long)v->num_val;
            any = true;
        } else {
            amounts[i] = 0;
        }
    }
    if (!any) return NULL;
    const json_t *call_role = om_get(in, "call_role");
    const json_t *provider = om_get(in, "provider");
    json_t *o = json_object();
    /* aux_task: seam only for the auxiliary role. */
    if (om_str_eq(om_str(call_role), "auxiliary")) {
        json_t *aux = om_aux_name(io, om_get(in, "aux_task"));
        if (om_is_raise(aux)) { json_free(o); return aux; }
        json_set(o, "aux_task", aux ? aux : json_null());
    } else {
        json_set(o, "aux_task", json_string("none"));
    }
    json_set(o, "call_role", call_role ? json_copy(call_role) : json_null());
    /* model: provider_identifier(provider) fires BEFORE model_metric_name. */
    json_t *p1 = om_provider_identifier_core(io, provider);
    if (om_is_raise(p1)) { json_free(o); return p1; }
    json_t *model = om_model_name(io, om_get(in, "model"), p1);
    json_free(p1);
    if (om_is_raise(model)) { json_free(o); return model; }
    json_set(o, "model", model ? model : json_null());
    /* provider: a SECOND provider_identifier(provider) call. */
    json_t *p2 = om_provider_identifier_core(io, provider);
    if (om_is_raise(p2)) { json_free(o); return p2; }
    json_set(o, "provider", p2 ? p2 : json_null());
    for (size_t i = 0; i < OM_N(OM_USAGE_KEYS); i++)
        json_set(o, OM_USAGE_KEYS[i].token_type, json_number((double)amounts[i]));
    return o;
}

/* PoP: om_setup_completed_fields @ hermes_cli/observability/shared_metrics_fields.py:setup_completed_fields */
static json_t *om_setup_completed_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *value = om_norm(io, om_get(in, "surface"));
    if (om_is_raise(value)) return value;
    json_t *o = json_object();
    /* provider key evaluated first: truthiness of the RAW provider. */
    const json_t *provider = om_get(in, "provider");
    if (om_truth(provider)) {
        json_t *p = om_provider_identifier_core(io, provider);
        if (om_is_raise(p)) { json_free(o); json_free(value); return p; }
        json_set(o, "provider", p ? p : json_null());
    } else {
        json_set(o, "provider", json_string("none"));
    }
    json_t *uh = om_set_hashable(value);
    if (uh) { json_free(o); json_free(value); return uh; }
    json_set(o, "surface",
             om_str_in(value, OM_SETUP_SURFACES, OM_N(OM_SETUP_SURFACES))
                 ? json_copy(value) : json_string("other"));
    json_free(value);
    return o;
}

static json_t *om_compression_failure_class_core(om_io_t *io, const char *outcome,
                                                 const json_t *failure_class)
{
    /* _norm(failure_class if isinstance(failure_class, str) else None) */
    json_t *nv = om_norm(io, om_is_str(failure_class) ? failure_class : NULL);
    if (om_is_raise(nv)) return nv;
    json_t *bad = om_split_carrier(nv);
    if (bad) { json_free(nv); return bad; }
    char *value = om_split_colon(nv->str_val);
    json_free(nv);
    if (!value) return om_mkraise("MemoryError", "out of memory");
    if (om_str_eq(outcome, "success")) {
        for (size_t i = 0; i < OM_N(OM_COMPRESSION_FAILURE_CLASS_ALIASES); i++)
            if (om_str_eq(value, OM_COMPRESSION_FAILURE_CLASS_ALIASES[i].from)) {
                char *nv2 = strdup(OM_COMPRESSION_FAILURE_CLASS_ALIASES[i].to);
                free(value);
                value = nv2;
                break;
            }
        if (value && (value[0] == '\0' || om_str_eq(value, "none"))) {
            free(value);
            return json_string("none");
        }
    }
    if (!value || value[0] == '\0') {
        free(value);
        return json_string("unknown");
    }
    bool ok = false;
    for (size_t i = 0; i < OM_N(OM_COMPRESSION_FAILURE_CLASSES); i++)
        if (om_str_eq(value, OM_COMPRESSION_FAILURE_CLASSES[i])) { ok = true; break; }
    if (om_str_eq(value, "none") || om_str_eq(value, "unknown")) ok = false;
    json_t *r = json_string(ok ? value : "other");
    free(value);
    return r;
}

/* PoP: om_compression_failure_class @ hermes_cli/observability/shared_metrics_fields.py:compression_failure_class */
static json_t *om_compression_failure_class(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *outcome = om_str(om_get(in, "outcome"));
    return om_compression_failure_class_core(io, outcome ? outcome : "",
                                             om_get(in, "failure_class"));
}

/* PoP: om_compression_fields @ hermes_cli/observability/shared_metrics_fields.py:compression_fields */
static json_t *om_compression_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *ov = om_norm(io, om_get(in, "outcome"));
    if (om_is_raise(ov)) return ov;
    const char *outcome_value = "failed";
    const char *ovs = om_str(ov);
    if (ovs)
        for (size_t i = 0; i < OM_N(OM_COMPRESSION_OUTCOMES); i++)
            if (om_str_eq(ovs, OM_COMPRESSION_OUTCOMES[i].key)) {
                outcome_value = OM_COMPRESSION_OUTCOMES[i].bucket;
                break;
            }
    json_free(ov);
    json_t *o = json_object();
    /* dict value order: context_fill_bucket, failure_class, outcome, trigger. */
    json_t *cf = om_context_fill_core(io, om_get(in, "tokens_before"),
                                      om_get(in, "context_length"));
    if (om_is_raise(cf)) { json_free(o); return cf; }
    json_set(o, "context_fill_bucket", cf);
    json_t *fc = om_compression_failure_class_core(io, outcome_value,
                                                   om_get(in, "failure_class"));
    if (om_is_raise(fc)) { json_free(o); return fc; }
    json_set(o, "failure_class", fc);
    json_set(o, "outcome", json_string(outcome_value));
    json_t *tv = om_norm(io, om_get(in, "trigger"));
    if (om_is_raise(tv)) { json_free(o); return tv; }
    const char *trigger = "other";
    const char *tvs = om_str(tv);
    if (tvs)
        for (size_t i = 0; i < OM_N(OM_COMPRESSION_TRIGGERS); i++)
            if (om_str_eq(tvs, OM_COMPRESSION_TRIGGERS[i].key)) {
                trigger = OM_COMPRESSION_TRIGGERS[i].bucket;
                break;
            }
    json_free(tv);
    json_set(o, "trigger", json_string(trigger));
    return o;
}

/* PoP: om_model_switch_fields @ hermes_cli/observability/shared_metrics_fields.py:model_switch_fields */
static json_t *om_model_switch_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *o = json_object();
    json_t *es = om_exec_surface(io, om_get(in, "surface"));
    if (om_is_raise(es)) { json_free(o); return es; }
    json_set(o, "execution_surface", es ? es : json_null());
    json_t *fp = om_provider_identifier_core(io, om_get(in, "from_provider"));
    if (om_is_raise(fp)) { json_free(o); return fp; }
    json_set(o, "from_provider", fp ? fp : json_null());
    json_t *tp = om_provider_identifier_core(io, om_get(in, "to_provider"));
    if (om_is_raise(tp)) { json_free(o); return tp; }
    json_set(o, "to_provider", tp ? tp : json_null());
    return o;
}

/* PoP: om_fallback_fields @ hermes_cli/observability/shared_metrics_fields.py:fallback_fields */
static json_t *om_fallback_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    /* value = getattr(reason, "value", reason): attribute doubles carry value;
     * a present-null value stays None; scalars pass through. */
    const json_t *reason = om_get(in, "reason");
    const json_t *value = reason;
    if (om_is_obj(reason)) {
        const json_t *v = om_get(reason, "value");
        if (v) value = v;
    }
    json_t *o = json_object();
    json_t *ec = om_model_error_class(io, value);
    if (om_is_raise(ec)) { json_free(o); return ec; }
    json_set(o, "error_class", ec ? ec : json_null());
    json_t *fp = om_provider_identifier_core(io, om_get(in, "from_provider"));
    if (om_is_raise(fp)) { json_free(o); return fp; }
    json_set(o, "from_provider", fp ? fp : json_null());
    json_t *tp = om_provider_identifier_core(io, om_get(in, "to_provider"));
    if (om_is_raise(tp)) { json_free(o); return tp; }
    json_set(o, "to_provider", tp ? tp : json_null());
    return o;
}

/* PoP: om_slash_command_fields @ hermes_cli/observability/shared_metrics_fields.py:slash_command_fields */
static json_t *om_slash_command_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *o = json_object();
    json_t *cn = om_slash_name(io, om_get(in, "command"));
    if (om_is_raise(cn)) { json_free(o); return cn; }
    json_set(o, "command", cn ? cn : json_null());
    json_t *es = om_exec_surface(io, om_get(in, "surface"));
    if (om_is_raise(es)) { json_free(o); return es; }
    json_set(o, "execution_surface", es ? es : json_null());
    return o;
}

/* PoP: om_tagged_failure_class @ hermes_cli/observability/shared_metrics_fields.py:tagged_failure_class */
static json_t *om_tagged_failure_class(om_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return om_tagged_core(om_get(in, "error"));
}

/* PoP: om_exception_failure_class @ hermes_cli/observability/shared_metrics_fields.py:exception_failure_class */
static json_t *om_exception_failure_class(om_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return om_exception_failure_class_core(om_get(in, "error"));
}

static json_t *om_extension_failure_class_core(om_io_t *io, const char *kind,
                                               const char *outcome,
                                               const json_t *failure_class,
                                               const json_t *error)
{
    if (om_str_eq(outcome, "success")) return json_string("none");
    const json_t *value = NULL;
    if (om_is_str(failure_class) && failure_class->str_val && failure_class->str_val[0])
        value = failure_class;
    json_t *owned = NULL;
    if (!value && om_is_obj(error) && om_truth(om_get(error, "__exc__"))) {
        owned = om_exception_failure_class_core(error);
        value = owned;
    }
    json_t *nv = om_norm(io, value);
    if (om_is_raise(nv)) { json_free(owned); return nv; }
    json_t *bad = om_split_carrier(nv);
    if (bad) { json_free(nv); json_free(owned); return bad; }
    char *val = om_split_colon(nv->str_val);
    json_free(nv); json_free(owned);
    if (!val) return om_mkraise("MemoryError", "out of memory");
    /* contract.EXTENSION_KIND_FAILURE_CLASSES[kind] - {"none"}: an unknown
     * kind raises KeyError AFTER the _norm seam fired. */
    const char *const *extra = NULL; size_t nextra = 0;
    if (om_str_eq(kind, "skill")) { extra = OM_EXT_SKILL_EXTRA; nextra = OM_N(OM_EXT_SKILL_EXTRA); }
    else if (om_str_eq(kind, "plugin")) { extra = OM_EXT_PLUGIN_EXTRA; nextra = OM_N(OM_EXT_PLUGIN_EXTRA); }
    else if (om_str_eq(kind, "mcp_server")) { extra = OM_EXT_MCP_EXTRA; nextra = OM_N(OM_EXT_MCP_EXTRA); }
    else {
        char *m = om_xfmt("'%s'", kind);
        json_t *r = om_mkraise("KeyError", m ? m : "'?'");
        free(m); free(val);
        return r;
    }
    bool ok = false;
    for (size_t i = 0; i < OM_N(OM_EXT_COMMON_FAILURE_CLASSES); i++)
        if (om_str_eq(val, OM_EXT_COMMON_FAILURE_CLASSES[i])) { ok = true; break; }
    if (!ok)
        for (size_t i = 0; i < nextra; i++)
            if (om_str_eq(val, extra[i])) { ok = true; break; }
    if (om_str_eq(val, "none")) ok = false;
    json_t *r = json_string(ok ? val : "other");
    free(val);
    return r;
}

/* PoP: om_extension_failure_class @ hermes_cli/observability/shared_metrics_fields.py:extension_failure_class */
static json_t *om_extension_failure_class(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *kind = om_str(om_get(in, "kind"));
    const char *outcome = om_str(om_get(in, "outcome"));
    return om_extension_failure_class_core(io, kind ? kind : "", outcome ? outcome : "",
                                           om_get(in, "failure_class"), om_get(in, "error"));
}

static json_t *om_extension_registry_core(om_io_t *io, const char *kind,
                                          const json_t *registry)
{
    if (!om_str_eq(kind, "skill")) return json_string("none");
    json_t *nv = om_norm(io, registry);
    if (om_is_raise(nv)) return nv;
    /* value = _norm(registry) or "none" */
    const char *value = om_str(nv);
    if (!value || !value[0]) value = "none";
    for (size_t i = 0; i < OM_N(OM_SOURCE_ID_ALIASES); i++)
        if (om_str_eq(value, OM_SOURCE_ID_ALIASES[i].from)) {
            value = OM_SOURCE_ID_ALIASES[i].to;
            break;
        }
    bool ok = false;
    for (size_t i = 0; i < OM_N(OM_EXTENSION_REGISTRIES); i++)
        if (om_str_eq(value, OM_EXTENSION_REGISTRIES[i])) { ok = true; break; }
    json_t *r = json_string(ok ? value : "other");   /* copy BEFORE freeing nv */
    json_free(nv);
    return r;
}

/* PoP: om_extension_registry @ hermes_cli/observability/shared_metrics_fields.py:extension_registry */
static json_t *om_extension_registry(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *kind = om_str(om_get(in, "kind"));
    return om_extension_registry_core(io, kind ? kind : "", om_get(in, "registry"));
}

/* PoP: om_extension_install_fields @ hermes_cli/observability/shared_metrics_fields.py:extension_install_fields */
static json_t *om_extension_install_fields(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *kv = om_norm(io, om_get(in, "kind"));
    if (om_is_raise(kv)) return kv;
    json_t *uh = om_set_hashable(kv);
    if (uh) { json_free(kv); return uh; }
    if (!om_str_in(kv, OM_EXTENSION_KINDS, OM_N(OM_EXTENSION_KINDS))) {
        json_free(kv);
        return NULL;
    }
    json_t *sv = om_norm(io, om_get(in, "source"));
    if (om_is_raise(sv)) { json_free(kv); return sv; }
    json_t *ov = om_norm(io, om_get(in, "outcome"));
    if (om_is_raise(ov)) { json_free(kv); json_free(sv); return ov; }
    const char *outcome_value =
        (om_str_eq(om_str(ov), "success") || om_str_eq(om_str(ov), "ok")
         || om_str_eq(om_str(ov), "installed")) ? "success" : "failed";
    json_free(ov);
    json_t *o = json_object();
    json_set(o, "kind", json_copy(kv));
    json_t *name = om_ext_name(io, kv->str_val, om_get(in, "name"));
    if (om_is_raise(name)) { json_free(o); json_free(kv); json_free(sv); return name; }
    json_set(o, "name", name ? name : json_null());
    json_set(o, "outcome", json_string(outcome_value));
    json_t *uh2 = om_set_hashable(sv);
    if (uh2) { json_free(o); json_free(kv); json_free(sv); return uh2; }
    json_set(o, "source",
             om_str_in(sv, OM_EXTENSION_SOURCES, OM_N(OM_EXTENSION_SOURCES))
                 ? json_copy(sv) : json_string("other"));
    json_free(sv);
    json_t *fc = om_extension_failure_class_core(io, kv->str_val, outcome_value,
                                                 om_get(in, "failure_class"),
                                                 om_get(in, "error"));
    if (om_is_raise(fc)) { json_free(o); json_free(kv); return fc; }
    json_set(o, "failure_class", fc);
    json_t *reg = om_extension_registry_core(io, kv->str_val, om_get(in, "registry"));
    if (om_is_raise(reg)) { json_free(o); json_free(kv); return reg; }
    json_set(o, "registry", reg);
    json_free(kv);
    return o;
}

static json_t *om_long_session_core(om_io_t *io, const json_t *d)
{
    (void)io;
    json_t *bad = om_get_carrier(d);
    if (bad) return bad;
    const json_t *v = om_get(d, "turn_count_bucket");
    json_t *uh = om_set_hashable(v);
    if (uh) return uh;
    static const char *small[] = { "0", "1", "2", "3_to_5" };
    return json_bool(!om_str_in(v, small, OM_N(small)));
}

/* PoP: om_long_session @ hermes_cli/observability/shared_metrics_fields.py:_long_session */
static json_t *om_long_session(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return om_long_session_core(io, om_get(in, "d"));
}

/* fields._MILESTONE_RULES predicates (source order per metric). */
enum {
    OM_PRED_ALWAYS = 0,
    OM_PRED_PLATFORM,          /* d.get("platform") not in {None, "none"} */
    OM_PRED_SCHEDULED,         /* d.get("entrypoint") == "scheduled_task" */
    OM_PRED_OUTCOME_SUCCESS,   /* d.get("outcome") == "success" */
    OM_PRED_MCP,               /* success and tool_name == "mcp" */
    OM_PRED_DELEGATE,          /* success and tool_name == "delegate_task" */
    OM_PRED_USER_SKILL,        /* signals.user_created_skill(d) — SEAM */
    OM_PRED_REUSED,            /* d.get("reuse_state") == "reused" */
    OM_PRED_LONG_SESSION,      /* _long_session(d) — internal */
};
typedef struct { const char *metric; const char *milestone; int pred; } om_rule_t;
static const om_rule_t OM_MILESTONE_RULES[] = {
    {OM_TASK_STARTED_METRIC, "first_task_started", OM_PRED_ALWAYS},
    {OM_TASK_STARTED_METRIC, "first_gateway_message", OM_PRED_PLATFORM},
    {OM_TASK_STARTED_METRIC, "first_scheduled_task", OM_PRED_SCHEDULED},
    {OM_TASK_FINISHED_METRIC, "first_task_success", OM_PRED_OUTCOME_SUCCESS},
    {OM_TOOL_USAGE_METRIC, "first_tool_success", OM_PRED_OUTCOME_SUCCESS},
    {OM_TOOL_USAGE_METRIC, "first_mcp_tool_success", OM_PRED_MCP},
    {OM_TOOL_USAGE_METRIC, "first_delegation", OM_PRED_DELEGATE},
    {OM_SKILL_LIFECYCLE_METRIC, "first_skill_created", OM_PRED_USER_SKILL},
    {OM_SKILL_LOAD_METRIC, "first_skill_reused", OM_PRED_REUSED},
    {OM_SESSION_METRIC, "first_long_session", OM_PRED_LONG_SESSION},
    {OM_SETUP_COMPLETED_METRIC, "setup_completed", OM_PRED_ALWAYS},
};

/* PoP: om_milestones_for @ hermes_cli/observability/shared_metrics_fields.py:milestones_for */
static json_t *om_milestones_for(om_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *metric = om_str(om_get(in, "metric_name"));
    const json_t *dims = om_get(in, "dimensions");   /* absent -> {} */
    json_t *out = json_array();
    for (size_t i = 0; i < OM_N(OM_MILESTONE_RULES); i++) {
        const om_rule_t *r = &OM_MILESTONE_RULES[i];
        if (!om_str_eq(metric ? metric : "", r->metric)) continue;
        bool reached = false;
        json_t *err = NULL;
        switch (r->pred) {
        case OM_PRED_ALWAYS:
            reached = true;
            break;
        case OM_PRED_PLATFORM: {
            err = om_get_carrier(dims);
            if (err) break;
            const json_t *p = om_get(dims, "platform");
            err = om_set_hashable(p);
            if (err) break;
            reached = !(om_is_null(p) || om_str_eq(om_str(p), "none"));
            break;
        }
        case OM_PRED_SCHEDULED:
            err = om_get_carrier(dims);
            if (err) break;
            reached = om_str_eq(om_str(om_get(dims, "entrypoint")), "scheduled_task");
            break;
        case OM_PRED_OUTCOME_SUCCESS:
            err = om_get_carrier(dims);
            if (err) break;
            reached = om_str_eq(om_str(om_get(dims, "outcome")), "success");
            break;
        case OM_PRED_MCP:
            err = om_get_carrier(dims);
            if (err) break;
            reached = om_str_eq(om_str(om_get(dims, "outcome")), "success")
                   && om_str_eq(om_str(om_get(dims, "tool_name")), "mcp");
            break;
        case OM_PRED_DELEGATE:
            err = om_get_carrier(dims);
            if (err) break;
            reached = om_str_eq(om_str(om_get(dims, "outcome")), "success")
                   && om_str_eq(om_str(om_get(dims, "tool_name")), "delegate_task");
            break;
        case OM_PRED_USER_SKILL: {
            json_t *res = om_user_created_skill(io, dims);
            if (om_is_raise(res)) { err = res; break; }
            reached = om_truth(res);
            json_free(res);
            break;
        }
        case OM_PRED_REUSED:
            err = om_get_carrier(dims);
            if (err) break;
            reached = om_str_eq(om_str(om_get(dims, "reuse_state")), "reused");
            break;
        case OM_PRED_LONG_SESSION: {
            json_t *res = om_long_session_core(io, dims);
            if (om_is_raise(res)) { err = res; break; }
            reached = om_truth(res);
            json_free(res);
            break;
        }
        default:
            break;
        }
        if (err) { json_free(out); return err; }
        if (reached) json_append(out, json_string(r->milestone));
    }
    return out;
}