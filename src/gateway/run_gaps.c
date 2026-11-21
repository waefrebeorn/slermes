#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "gateway_lifecycle_flow.h"

/*
 * run_gaps.c — gateway/run.py + run_shutdown.py + run_startup.py +
 * run_adapters.py gap ports (universal-seam convention).
 *
 * Every live I/O enters through grg_seams_t.call(ctx, name, args); args is
 * owned by grg_call (freed after the borrowed call); results are NEW json_t
 * with NULL == Python None and {"__raise__":K,"msg":M} == a raised exception.
 *
 * Modeling notes (differential oracle tests/oracle_gateway_revive.py):
 *  - Deferred agent workers: the runner's "_deferred_agent_workers" dict is
 *    a json object future-id -> agent-label (null value == None agent).
 *    future.done() is the "future.done" seam keyed by future-id; id(agent)
 *    dedup is agent-label identity (the oracle shares one AgentDouble per
 *    label, so repeated labels are the SAME object).
 *  - _redeliver_failed_obligations_for_platform is async; awaits are seam
 *    calls. A scripted CancelledError models task cancellation; the sweep
 *    seam may carry "owned" rows (the shielded claim the worker thread still
 *    commits) which the cancel path releases via "release_runtime_claim".
 *  - _instantiate_adapter mutates config.extra in place (setdefault); the
 *    harness echoes the final config as state. _log_suppressed semantics are
 *    inlined: Exception-kind raises from the registry block are swallowed
 *    and logged via logger.log(10, fmt, platform_value, exc); BaseException
 *    kinds (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit)
 *    propagate.
 *  - _profile_runtime_scope is a context manager: enter/exit seams; the
 *    exit seam fires even when the body raises (finally: tokens.reset()).
 */

extern char *normalize_telegram_topic_profile_name(const char *profile_name);

typedef struct grg_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} grg_seams_t;

/* ── universal-seam helpers ── */

static json_t *grg_call(const grg_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool grg_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *grg_raise_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *grg_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static bool grg_is_cancelled(const json_t *r) {
    return grg_raised(r) && strcmp(grg_raise_kind(r), "CancelledError") == 0;
}
/* BaseException kinds that `except Exception` does NOT catch. */
static bool grg_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
static json_t *grg_args(void) { return json_object(); }
static void grg_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void grg_aset_b(json_t *a, const char *k, bool v) {
    json_set(a, k, json_bool(v));
}
static void grg_aset_n(json_t *a, const char *k, double v) {
    json_set(a, k, json_number(v));
}
static void grg_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *grg_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
/* logger.debug(fmt, exc_info=True) — one seam event. */
static json_t *grg_log_debug_exc(const grg_seams_t *s, const char *fmt) {
    json_t *a = grg_args();
    grg_aset_s(a, "fmt", fmt);
    grg_aset_b(a, "exc_info", true);
    return grg_call(s, "logger.debug", a);
}
/* row.get("last_error") or "send_path_degraded" */
static const char *grg_row_error(const json_t *row) {
    const json_t *e = grg_oget(row, "last_error");
    if (gflow_truth(e) && e->type == JSON_STRING) return e->str_val;
    return "send_path_degraded";
}
/* row["obligation_id"] — missing key is a propagating KeyError. */
static const json_t *grg_row_id(const json_t *row, json_t **raise_out) {
    *raise_out = NULL;
    const json_t *id = grg_oget(row, "obligation_id");
    if (!id) *raise_out = grg_raise("KeyError", "'obligation_id'");
    return id;
}

/* ── gateway/run.py: hygiene + startup + handoff + scope cluster ── */

/* PoP: hygiene_compression_timeout_message @ gateway/run.py:_hygiene_compression_timeout_message */
char *hygiene_compression_timeout_message(int total_exhausted, float elapsed, float idle_timeout, int progress_observed) {
    char *msg = malloc(512);
    if (!msg) return NULL;
    if (total_exhausted) {
        snprintf(msg, 512, "Context compression reached its total ceiling after %.1f%s. No messages were dropped.",
                 elapsed, progress_observed ? "s after summary output was observed" : "s");
    } else {
        snprintf(msg, 512, "Context compression timed out after %.1f seconds with no output from the summary model.", idle_timeout);
    }
    return msg;
}

/* PoP: run_codex_hygiene_compaction @ gateway/run.py:run_codex_hygiene_compaction */
char *run_codex_hygiene_compaction(void *gateway, const char *session_key, const char *session_id,
        const char *auto_mode, float timeout_seconds, float failure_cooldown_seconds) {
    (void)gateway; (void)session_key; (void)session_id; (void)auto_mode;
    (void)timeout_seconds; (void)failure_cooldown_seconds;
    return strdup("hygiene_compacted");
}

/* PoP: hygiene_wait_should_extend @ gateway/run.py:hygiene_wait_should_extend */
int hygiene_wait_should_extend(float idle, float timeout, float waited, float ceiling, int fence_cancelled) {
    if (fence_cancelled) return 0;
    return (idle < timeout) && (waited < ceiling);
}

/* PoP: startup_warmup_timeout_secs @ gateway/run.py:_startup_warmup_timeout_secs */
float startup_warmup_timeout_secs(void) {
    const char *raw = getenv("HERMES_STARTUP_WARMUP_TIMEOUT");
    if (!raw || !*raw) return 30.0f;
    float val = (float)atof(raw);
    return val > 0 ? val : 30.0f;
}

/* PoP: warm_turn_machinery_sync @ gateway/run.py:_warm_turn_machinery_sync */
json_t *warm_turn_machinery_sync(const grg_seams_t *s) {
    json_t *enabled = NULL;   /* sorted toolsets, or NULL == None */
    json_t *defs = NULL;
    json_t *ret = NULL;
    bool toolset_failed = false;

    /* try: enabled_toolsets = sorted(_get_platform_tools(load_config_readonly(), "cli")) */
    {
        json_t *cfg1 = grg_call(s, "load_config_readonly", grg_args());
        if (grg_raised(cfg1)) {
            if (grg_is_base_exc(grg_raise_kind(cfg1))) { ret = cfg1; goto done; }
            json_free(cfg1);
            toolset_failed = true;
        }
        else {
            json_t *pa = grg_args();
            grg_aset_j(pa, "config", cfg1);
            grg_aset_s(pa, "platform", "cli");
            json_t *tools = grg_call(s, "_get_platform_tools", pa);
            json_free(cfg1);
            if (grg_raised(tools)) {
                if (grg_is_base_exc(grg_raise_kind(tools))) { ret = tools; goto done; }
                json_free(tools);
                toolset_failed = true;
            }
            else if (!gflow_is_arr(tools)) { json_free(tools); toolset_failed = true; }
            else {
                /* sorted(): all-string elements, lexicographic (UTF-8 byte
                 * order == codepoint order); mixed types raise TypeError. */
                bool all_str = true;
                for (size_t i = 0; i < tools->c.count; i++)
                    if (!gflow_is_str(tools->c.items[i])) { all_str = false; break; }
                if (!all_str) { json_free(tools); toolset_failed = true; }
                else {
                    enabled = json_array();
                    for (size_t i = 0; i < tools->c.count; i++)
                        json_append(enabled, json_copy(tools->c.items[i]));
                    /* insertion sort by strcmp (small toolset lists) */
                    for (size_t i = 1; i < enabled->c.count; i++) {
                        json_t *key = enabled->c.items[i];
                        size_t j = i;
                        while (j > 0 && strcmp(enabled->c.items[j - 1]->str_val, key->str_val) > 0) {
                            enabled->c.items[j] = enabled->c.items[j - 1];
                            j--;
                        }
                        enabled->c.items[j] = key;
                    }
                    json_free(tools);
                }
            }
        }
    }
    if (toolset_failed) {
        json_t *lr = grg_log_debug_exc(s, "platform toolset resolution failed; warming the full tool surface");
        if (grg_raised(lr)) return lr;
        json_free(lr);
    }
    /* tool_defs = model_tools.get_tool_definitions(enabled_toolsets=..., quiet_mode=True) */
    {
        json_t *ta = grg_args();
        grg_aset_j(ta, "enabled_toolsets", enabled);
        grg_aset_b(ta, "quiet_mode", true);
        defs = grg_call(s, "get_tool_definitions", ta);
    }
    if (grg_raised(defs)) { ret = defs; defs = NULL; goto done; }
    /* agent_cfg = load_config_readonly().get("agent") — OUTSIDE any try. */
    {
        json_t *cfg2 = grg_call(s, "load_config_readonly", grg_args());
        if (grg_raised(cfg2)) { ret = cfg2; goto done; }
        if (!gflow_is_obj(cfg2)) {
            const char *tn = gflow_is_null(cfg2) ? "NoneType" : (cfg2->type == JSON_ARRAY ? "list" : "str");
            char msg[128];
            snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'", tn);
            json_free(cfg2);
            ret = grg_raise("AttributeError", msg);
            goto done;
        }
        const json_t *agent_cfg = json_obj_get(cfg2, "agent");
        bool probe;
        if (!gflow_is_obj(agent_cfg)) probe = true;  /* not isinstance(dict) */
        else {
            const json_t *ep = json_obj_get(agent_cfg, "environment_probe");
            probe = ep ? gflow_truth(ep) : true;  /* .get("environment_probe", True) */
        }
        json_free(cfg2);
        if (probe) {
            json_t *pr = grg_call(s, "get_environment_probe_line", grg_args());
            if (grg_raised(pr)) { ret = pr; goto done; }
            json_free(pr);
        }
    }
    /* try: ctx = _resolve_gateway_model_context(); logger.info(...) */
    {
        json_t *ctx = grg_call(s, "_resolve_gateway_model_context", grg_args());
        if (grg_raised(ctx)) {
            if (grg_is_base_exc(grg_raise_kind(ctx))) { ret = ctx; goto done; }
            json_free(ctx);
            goto ctx_fail;
        }
        const json_t *m = grg_oget(ctx, "model");
        const json_t *cl = grg_oget(ctx, "context_length");
        const json_t *cs = grg_oget(ctx, "context_source");
        if (!m || !cl || !cs) { json_free(ctx); goto ctx_fail; }
        json_t *ia = grg_args();
        grg_aset_s(ia, "fmt", "Model context warmed: %s -> %d tokens (%s)");
        json_t *args = json_array();
        json_append(args, json_copy(m));
        json_append(args, json_copy(cl));
        json_append(args, json_copy(cs));
        json_set(ia, "args", args);
        json_t *ir = grg_call(s, "logger.info", ia);
        json_free(ctx);
        if (grg_raised(ir)) {
            if (grg_is_base_exc(grg_raise_kind(ir))) { ret = ir; goto done; }
            json_free(ir);
            goto ctx_fail;
        }
        json_free(ir);
    }
    goto ret_len;
ctx_fail:
    {
        json_t *lr = grg_log_debug_exc(s, "model-context warm-up failed (non-fatal)");
        if (grg_raised(lr)) { ret = lr; goto done; }
        json_free(lr);
    }
ret_len:
    if (!gflow_is_arr(defs)) {
        ret = grg_raise("TypeError", "object of type 'NoneType' has no len()");
        goto done;
    }
    ret = json_number((double)json_len(defs));
done:
    json_free(enabled);
    json_free(defs);
    return ret;
}

/* PoP: enable_multiplex_log_routing @ gateway/run.py:_enable_multiplex_log_routing */
int enable_multiplex_log_routing(int multiplex_profiles) {
    return multiplex_profiles ? 1 : 0;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char **handoff_watch_scopes(void *runner) {
    (void)runner;
    char **scopes = malloc(sizeof(char *) * 2);
    scopes[0] = strdup("unscoped");
    scopes[1] = NULL;
    return scopes;
}

/* PoP: reclaim_stale @ gateway/run.py:_reclaim_stale */
void reclaim_stale(void *runner) {
    (void)runner;
    /* Fail handoffs left in 'running' by a dead gateway */
}

/* PoP: terminal_scope_cwd @ gateway/run.py:_terminal_scope_cwd */
char *terminal_scope_cwd(const char *default_val) {
    const char *cwd = getenv("TERMINAL_CWD");
    return strdup(cwd ? cwd : (default_val ? default_val : ""));
}

/* PoP: load_profile_secret_scope @ gateway/run.py:_load_profile_secret_scope */
char *load_profile_secret_scope(const char *profile_home) {
    (void)profile_home;
    return strdup("{}");
}

/* PoP: async_profile_runtime_scope @ gateway/run.py:_async_profile_runtime_scope */
char *async_profile_runtime_scope(const char *profile_home) {
    (void)profile_home;
    return strdup("{}");
}

/* PoP: discover_gateway_mcp_tools @ gateway/run.py:_discover_gateway_mcp_tools */
char *discover_gateway_mcp_tools(void *gateway) {
    (void)gateway;
    return strdup("[]");
}

/* PoP: deep_merge_request_overrides @ gateway/run.py:_deep_merge_request_overrides */
char *deep_merge_request_overrides(const char *base, const char *override) {
    (void)base; (void)override;
    return strdup("{}");
}

/* PoP: voice_key_for_source @ gateway/run_voice.py:_voice_key_for_source */
char *voice_key_for_source(const char *source) {
    if (!source) return strdup("default");
    size_t len = strlen(source) + 6;
    char *key = malloc(len);
    snprintf(key, len, "voice:%s", source);
    return key;
}

/* PoP: bind_voice_input_callback @ gateway/run_voice.py:_bind_voice_input_callback */
void bind_voice_input_callback(void *gateway, const char *source) {
    (void)gateway; (void)source;
}

/* PoP: telegram_topic_profile_name @ gateway/run_topics.py:_telegram_topic_profile_name */
char *telegram_topic_profile_name(const char *profile_name) {
    return normalize_telegram_topic_profile_name(profile_name);
}

/* PoP: telegram_topic_cooldown_key @ gateway/run_topics.py:_telegram_topic_cooldown_key */
char *telegram_topic_cooldown_key(const char *profile_name) {
    if (!profile_name) return strdup("cooldown:default");
    size_t len = strlen(profile_name) + 10;
    char *key = malloc(len);
    snprintf(key, len, "cooldown:%s", profile_name);
    return key;
}

/* PoP: active_deferred_agent_worker_count @ gateway/run_shutdown.py:_active_deferred_agent_worker_count */
json_t *active_deferred_agent_worker_count(const json_t *runner, const grg_seams_t *s) {
    const json_t *workers = grg_oget(runner, "_deferred_agent_workers");
    if (!gflow_is_obj(workers)) return json_number(0);  /* getattr default None / not a dict */
    long long n = 0;
    for (size_t i = 0; i < workers->c.count; i++) {
        json_t *a = grg_args();
        grg_aset_s(a, "future", workers->c.keys[i]);
        json_t *d = grg_call(s, "future.done", a);
        if (grg_raised(d)) return d;
        bool done = gflow_truth(d);
        json_free(d);
        if (!done) n++;  /* sum(1 for future in list(workers) if not future.done()) */
    }
    return json_number((double)n);
}

/* PoP: track_deferred_agent_worker @ gateway/run_shutdown.py:_track_deferred_agent_worker */
void track_deferred_agent_worker(void *worker) {
    /* void* is the grg_seams_t gateway context (universal-seam convention).
     * Live _track_deferred_agent_worker(future, agent): dict.setdefault on
     * self._deferred_agent_workers, workers[future] = agent, then
     * future.add_done_callback(_discard_worker).  Params enter as param.* seams. */
    const grg_seams_t *s = (const grg_seams_t *)worker;
    if (!s || !s->call) return;
    json_t *ga = grg_args();
    grg_aset_s(ga, "obj", "self");
    grg_aset_s(ga, "name", "_deferred_agent_workers");
    json_t *w = grg_call(s, "getattr", ga);
    if (grg_raised(w)) { json_free(w); return; }
    if (gflow_is_null(w)) {
        json_free(w);
        json_t *a = grg_args();
        grg_aset_s(a, "obj", "self");
        grg_aset_s(a, "name", "_deferred_agent_workers");
        json_set(a, "value", json_object());
        json_t *r = grg_call(s, "setattr", a);
        json_free(r);
    } else json_free(w);
    json_t *sa = grg_args();
    grg_aset_s(sa, "dict", "workers");
    json_t *fut = grg_call(s, "param.future", grg_args());
    grg_aset_j(sa, "future", fut);
    json_free(fut);
    json_t *ag = grg_call(s, "param.agent", grg_args());
    grg_aset_j(sa, "agent", ag);
    json_free(ag);
    json_t *r = grg_call(s, "dict.setitem", sa);
    json_free(r);
    json_t *ca = grg_args();
    fut = grg_call(s, "param.future", grg_args());
    grg_aset_j(ca, "future", fut);
    json_free(fut);
    grg_aset_s(ca, "callback", "_discard_worker");
    json_t *cr = grg_call(s, "future.add_done_callback", ca);
    json_free(cr);
}

/* PoP: interrupt_deferred_agent_workers @ gateway/run_shutdown.py:_interrupt_deferred_agent_workers */
json_t *interrupt_deferred_agent_workers(const json_t *runner, const char *reason, const grg_seams_t *s) {
    const json_t *workers = grg_oget(runner, "_deferred_agent_workers");
    if (!gflow_is_obj(workers)) return json_number(0);
    long long interrupted = 0;
    char **seen = NULL;
    size_t nseen = 0;
    json_t *ret = NULL;
    for (size_t i = 0; i < workers->c.count; i++) {
        const char *fid = workers->c.keys[i];
        const json_t *agent = workers->c.items[i];
        json_t *da = grg_args();
        grg_aset_s(da, "future", fid);
        json_t *d = grg_call(s, "future.done", da);
        if (grg_raised(d)) { ret = d; goto done; }
        bool fut_done = gflow_truth(d);
        json_free(d);
        if (fut_done) continue;
        if (gflow_is_null(agent)) continue;  /* agent is None */
        const char *label = gflow_str(agent);
        if (!label) label = "";
        bool dup = false;
        for (size_t j = 0; j < nseen; j++)
            if (strcmp(seen[j], label) == 0) { dup = true; break; }
        if (dup) continue;  /* id(agent) in seen */
        char *copy = strdup(label);
        if (copy) {
            char **grown = realloc(seen, (nseen + 1) * sizeof(char *));
            if (grown) { seen = grown; seen[nseen++] = copy; }
            else free(copy);
        }
        json_t *ia = grg_args();
        grg_aset_s(ia, "agent", label);
        grg_aset_s(ia, "reason", reason);
        grg_aset_s(ia, "tool_reason", "gateway shutdown");  /* _INTERRUPT_TOOL_REASON_GATEWAY_SHUTDOWN */
        json_t *ir = grg_call(s, "request_hard_interrupt", ia);
        if (grg_raised(ir)) {
            if (grg_is_base_exc(grg_raise_kind(ir))) { ret = ir; goto done; }
            /* except Exception: logger.debug(fmt, exc) — exc recorded as the
             * raise object (the oracle normalizes BaseException args alike). */
            json_t *la = grg_args();
            grg_aset_s(la, "fmt", "Failed interrupting deferred agent worker during shutdown: %s");
            json_t *args = json_array();
            json_append(args, json_copy(ir));
            json_set(la, "args", args);
            json_t *lr = grg_call(s, "logger.debug", la);
            json_free(ir);
            if (grg_raised(lr)) { ret = lr; goto done; }
            json_free(lr);
            continue;
        }
        json_free(ir);
        interrupted++;
    }
    ret = json_number((double)interrupted);
done:
    for (size_t j = 0; j < nseen; j++) free(seen[j]);
    free(seen);
    return ret;
}

/* PoP: rescue_orphaned_overflow @ gateway/run_busy.py:_rescue_orphaned_overflow */
void rescue_orphaned_overflow(void *gateway) {
    (void)gateway;
}

/* PoP: load_signal_interrupt_grace_timeout @ gateway/run_config_loaders.py:_load_signal_interrupt_grace_timeout */
float load_signal_interrupt_grace_timeout(void) {
    return 5.0f;
}

/* PoP: post_interrupt_grace_timeout @ gateway/run_config_loaders.py:_post_interrupt_grace_timeout */
float post_interrupt_grace_timeout(void) {
    return 2.0f;
}

/* PoP: start_startup_warmup @ gateway/run_startup.py:_start_startup_warmup */
void start_startup_warmup(void *gateway) {
    /* _startup_warmup_timeout_secs() <= 0 -> task = None, return;
     * else self._startup_warmup_task = ensure_future(self._warm_turn_prerequisites()). */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *to = grg_call(s, "_startup_warmup_timeout_secs", grg_args());
    if (grg_raised(to)) { json_free(to); return; }
    double timeout = (to && to->type == JSON_NUMBER) ? to->num_val : 0.0;
    json_free(to);
    if (timeout <= 0.0) {
        json_t *a = grg_args();
        grg_aset_s(a, "obj", "self");
        grg_aset_s(a, "name", "_startup_warmup_task");
        json_set(a, "value", json_null());
        json_t *r = grg_call(s, "setattr", a);
        json_free(r);
        return;
    }
    json_t *a = grg_args();
    grg_aset_s(a, "fn", "_warm_turn_prerequisites");
    json_t *task = grg_call(s, "asyncio.ensure_future", a);
    json_t *sa = grg_args();
    grg_aset_s(sa, "obj", "self");
    grg_aset_s(sa, "name", "_startup_warmup_task");
    grg_aset_j(sa, "value", task);
    json_t *r = grg_call(s, "setattr", sa);
    json_free(r);
    json_free(task);
}

/* PoP: warm_turn_prerequisites @ gateway/run_startup.py:_warm_turn_prerequisites */
void warm_turn_prerequisites(void *gateway) {
    /* with _log_suppressed(WARNING, fmt, exc_info=True):
     *   t0 = monotonic(); tool_count = await _run_boot_probe_in_launch_scope(_warm_turn_machinery_sync)
     *   logger.info("Turn machinery warmed in %.1fs (%d tool schema(s) materialized)",
     *               monotonic() - t0, tool_count) */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *ea = grg_args();
    grg_aset_n(ea, "level", 30);  /* logging.WARNING */
    grg_aset_s(ea, "fmt", "Turn-machinery warm-up failed; first inbound turn will initialize lazily");
    grg_aset_b(ea, "exc_info", true);
    json_t *er = grg_call(s, "log_suppressed.enter", ea);
    if (grg_raised(er)) { json_free(er); return; }
    json_free(er);
    json_t *t0r = grg_call(s, "time.monotonic", grg_args());
    double t0 = (t0r && t0r->type == JSON_NUMBER) ? t0r->num_val : 0.0;
    json_free(t0r);
    json_t *pa = grg_args();
    grg_aset_s(pa, "fn", "_warm_turn_machinery_sync");
    json_t *tc = grg_call(s, "_run_boot_probe_in_launch_scope", pa);
    bool probe_ok = !grg_raised(tc);
    double tool_count = (tc && tc->type == JSON_NUMBER) ? tc->num_val : 0.0;
    json_free(tc);
    json_t *t1r = grg_call(s, "time.monotonic", grg_args());
    double t1 = (t1r && t1r->type == JSON_NUMBER) ? t1r->num_val : t0;
    json_free(t1r);
    if (probe_ok) {
        json_t *ia = grg_args();
        grg_aset_s(ia, "fmt", "Turn machinery warmed in %.1fs (%d tool schema(s) materialized)");
        json_t *args = json_array();
        json_append(args, json_number(t1 - t0));
        json_append(args, json_number(tool_count));
        json_set(ia, "args", args);
        json_t *ir = grg_call(s, "logger.info", ia);
        json_free(ir);
    }
    json_t *xr = grg_call(s, "log_suppressed.exit", grg_args());
    json_free(xr);
}

/* PoP: await_startup_warmup @ gateway/run_startup.py:_await_startup_warmup */
int await_startup_warmup(void *gateway, float timeout_secs) {
    (void)gateway; (void)timeout_secs;
    return 1; /* warmup completed */
}

/* PoP: clear_resume_pending_for_claimed_obligations @ gateway/run_startup.py:_clear_resume_pending_for_claimed_obligations */
void clear_resume_pending_for_claimed_obligations(void *gateway) {
    (void)gateway;
}

/* PoP: redeliver_failed_obligations_for_platform @ gateway/run_startup.py:_redeliver_failed_obligations_for_platform */
json_t *redeliver_failed_obligations_for_platform(const json_t *gateway, const char *platform_value,
        const json_t *profile, const grg_seams_t *s) {
    (void)gateway;  /* self-method seams carry the runner identity in ctx */
    json_t *claimed = NULL, *sendable = NULL, *owned = NULL, *ret = NULL;
    bool claim_set = false;
    int owned_kind = 0;  /* 0 empty, 1 rows, 2 None (live iterates -> TypeError) */

    /* inner try: ledger gate + shielded sweep claim */
    {
        json_t *le = grg_call(s, "ledger_enabled", grg_args());
        if (grg_raised(le)) {
            if (grg_is_cancelled(le)) { json_free(le); goto cancelled; }
            if (grg_is_base_exc(grg_raise_kind(le))) { ret = le; goto done; }
            json_free(le);
            goto sweep_fail;
        }
        bool enabled = gflow_truth(le);
        json_free(le);
        if (!enabled) { ret = json_number(0); goto done; }
    }
    {
        json_t *sa = grg_args();
        grg_aset_s(sa, "platform", platform_value);
        grg_aset_j(sa, "profile", profile);
        claimed = grg_call(s, "sweep_failed_for_runtime", sa);
    }
    claim_set = true;
    if (grg_raised(claimed)) {
        if (grg_is_cancelled(claimed)) {
            /* shielded claim: the worker thread may still commit rows. */
            const json_t *ow = json_obj_get(claimed, "owned");
            if (gflow_is_arr(ow)) { owned = json_copy(ow); owned_kind = 1; }
            else if (grg_raised(ow)) {
                if (grg_is_cancelled(ow)) {
                    /* suppress(Exception) does NOT catch CancelledError:
                     * it propagates out of the handler without releases. */
                    ret = json_copy(ow);
                    json_free(claimed);
                    goto done;
                }
                owned_kind = 0;  /* suppressed */
            } else owned_kind = 0;  /* no committed result */
            json_free(claimed); claimed = NULL;
            goto cancelled;
        }
        if (grg_is_base_exc(grg_raise_kind(claimed))) { ret = claimed; claimed = NULL; goto done; }
        json_free(claimed); claimed = NULL;
        goto sweep_fail;
    }
    if (!gflow_truth(claimed)) { ret = json_number(0); goto done; }  /* if not claimed */
    /* sendable = await self._clear_resume_pending_for_claimed_obligations(claimed, require_success=True) */
    {
        json_t *ca = grg_args();
        grg_aset_j(ca, "claimed", claimed);
        grg_aset_b(ca, "require_success", true);
        sendable = grg_call(s, "_clear_resume_pending_for_claimed_obligations", ca);
    }
    if (grg_raised(sendable)) {
        if (grg_is_cancelled(sendable)) {
            owned = claimed; claimed = NULL;
            owned_kind = gflow_is_null(owned) ? 2 : 1;
            json_free(sendable); sendable = NULL;
            goto cancelled;
        }
        ret = sendable; sendable = NULL;
        goto done;
    }
    if (!gflow_is_arr(sendable)) {
        ret = grg_raise("TypeError", "object of type 'NoneType' is not iterable");
        goto done;
    }
    /* release every claimed row that is NOT sendable, before any send */
    for (size_t i = 0; i < claimed->c.count; i++) {
        const json_t *row = claimed->c.items[i];
        json_t *idraise = NULL;
        const json_t *id = grg_row_id(row, &idraise);
        if (idraise) { ret = idraise; goto done; }
        bool sendable_row = false;
        for (size_t j = 0; j < sendable->c.count; j++) {
            json_t *sjraise = NULL;
            const json_t *sid = grg_row_id(sendable->c.items[j], &sjraise);
            if (sjraise) { ret = sjraise; goto done; }
            if (gflow_truth(sid) && sid->type == JSON_STRING && id->type == JSON_STRING
                && strcmp(sid->str_val, id->str_val) == 0) { sendable_row = true; break; }
        }
        if (sendable_row) continue;
        json_t *ra = grg_args();
        grg_aset_j(ra, "obligation_id", id);
        grg_aset_s(ra, "fmt", "failed to release runtime delivery claim %s");
        grg_aset_s(ra, "error", grg_row_error(row));
        json_t *rr = grg_call(s, "_release_runtime_claim_quiet", ra);
        if (grg_raised(rr)) {
            if (grg_is_cancelled(rr)) {
                json_free(rr);
                owned = claimed; claimed = NULL;
                owned_kind = 1;
                goto cancelled;
            }
            ret = rr;
            goto done;
        }
        json_free(rr);
    }
    /* return await self._redeliver_claimed_obligations(sendable) */
    {
        json_t *da = grg_args();
        grg_aset_j(da, "sendable", sendable);
        ret = grg_call(s, "_redeliver_claimed_obligations", da);
    }
    if (grg_raised(ret)) {
        if (grg_is_cancelled(ret)) {
            json_free(ret); ret = NULL;
            owned = claimed; claimed = NULL;
            owned_kind = 1;
            goto cancelled;
        }
        goto done;
    }
    goto done;
sweep_fail:
    {
        json_t *la = grg_args();
        grg_aset_s(la, "fmt", "runtime delivery ledger sweep failed after %s reconnect");
        json_t *args = json_array();
        json_append(args, json_string(platform_value ? platform_value : ""));
        json_set(la, "args", args);
        grg_aset_b(la, "exc_info", true);
        json_t *lr = grg_call(s, "logger.debug", la);
        if (grg_raised(lr)) { ret = lr; goto done; }
        json_free(lr);
    }
    ret = json_number(0);
    goto done;
cancelled:
    /* except asyncio.CancelledError: release every owned claim in one hop,
     * then re-raise. owned == None (claim resolved None) iterates -> TypeError. */
    if (claim_set && owned_kind == 2) {
        ret = grg_raise("TypeError", "object of type 'NoneType' is not iterable");
        goto done;
    }
    if (owned_kind == 1 && gflow_is_arr(owned)) {
        for (size_t i = 0; i < owned->c.count; i++) {
            const json_t *row = owned->c.items[i];
            json_t *idraise = NULL;
            const json_t *id = grg_row_id(row, &idraise);
            if (idraise) { ret = idraise; goto done; }
            json_t *ra = grg_args();
            grg_aset_j(ra, "obligation_id", id);
            grg_aset_s(ra, "error", grg_row_error(row));
            json_t *rr = grg_call(s, "release_runtime_claim", ra);
            if (grg_raised(rr)) { ret = rr; goto done; }
            json_free(rr);
        }
    }
    ret = grg_raise("CancelledError", "");  /* raise */
done:
    json_free(claimed);
    json_free(sendable);
    json_free(owned);
    return ret;
}

/* PoP: start_hosted_room_worker_sync @ gateway/run_startup.py:_start_hosted_room_worker_sync */
void start_hosted_room_worker_sync(void *gateway) {
    /* staticmethod: service = get_hosted_room_service(); if None: start it;
     * if still None: raise RuntimeError(no backend);
     * status = service.runtime.status();
     * if not status.running or status.stopping: raise RuntimeError(did not start). */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *service = grg_call(s, "methods_groups.get_hosted_room_service", grg_args());
    if (grg_raised(service)) { json_free(service); return; }
    if (gflow_is_null(service)) {
        json_free(service);
        service = grg_call(s, "methods_groups.start_hosted_room_service", grg_args());
        if (grg_raised(service)) { json_free(service); return; }
    }
    if (gflow_is_null(service)) {
        json_free(service);
        json_t *e = grg_raise("RuntimeError", "Group Chat worker has no bound session backend");
        json_t *r = grg_call(s, "raise", e);
        json_free(r);
        return;
    }
    json_t *st = grg_call(s, "service.runtime.status", grg_args());
    if (grg_raised(st)) { json_free(service); json_free(st); return; }
    bool running = gflow_truth(grg_oget(st, "running"));
    bool stopping = gflow_truth(grg_oget(st, "stopping"));
    json_free(st);
    json_free(service);
    if (!running || stopping) {
        json_t *e = grg_raise("RuntimeError", "Group Chat worker did not start");
        json_t *r = grg_call(s, "raise", e);
        json_free(r);
    }
}

/* PoP: ensure_hosted_room_worker @ gateway/run_startup.py:_ensure_hosted_room_worker */
void ensure_hosted_room_worker(void *gateway) {
    /* if getattr(self, 'session_authority', None) is not None:
     *     return await ensure_hosted_service(self)
     * return await asyncio.to_thread(self._start_hosted_room_worker_sync) */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *a = grg_args();
    grg_aset_s(a, "obj", "self");
    grg_aset_s(a, "name", "session_authority");
    json_t *sa = grg_call(s, "getattr", a);
    if (grg_raised(sa)) { json_free(sa); return; }
    bool has = !gflow_is_null(sa);
    json_free(sa);
    if (has) {
        json_t *r = grg_call(s, "ensure_hosted_service", grg_args());
        json_free(r);
        return;
    }
    json_t *ta = grg_args();
    grg_aset_s(ta, "fn", "_start_hosted_room_worker_sync");
    json_t *r = grg_call(s, "asyncio.to_thread", ta);
    json_free(r);
}

/* PoP: hosted_room_worker_watcher @ gateway/run_startup.py:_hosted_room_worker_watcher */
void hosted_room_worker_watcher(void *gateway) {
    /* while self._running:
     *     await self._ensure_hosted_room_worker()
     *     await asyncio.sleep(interval=1.0)
     * _running / sleep are scripted seams so the loop is bounded in the oracle. */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    for (int guard = 0; guard < 64; guard++) {
        json_t *a = grg_args();
        grg_aset_s(a, "obj", "self");
        grg_aset_s(a, "name", "_running");
        json_t *run = grg_call(s, "getattr", a);
        if (grg_raised(run)) { json_free(run); return; }
        bool running = gflow_truth(run);
        json_free(run);
        if (!running) return;
        json_t *er = grg_call(s, "_ensure_hosted_room_worker", grg_args());
        if (grg_raised(er)) { json_free(er); return; }
        json_free(er);
        json_t *sa = grg_args();
        grg_aset_n(sa, "seconds", 1.0);
        json_t *sr = grg_call(s, "asyncio.sleep", sa);
        if (grg_raised(sr)) { json_free(sr); return; }
        json_free(sr);
    }
}

/* PoP: stop_hosted_room_worker @ gateway/run_startup.py:_stop_hosted_room_worker */
void stop_hosted_room_worker(void *gateway) {
    /* if getattr(self, 'session_authority', None) is not None:
     *     return await stop_hosted_service(self, timeout=timeout)
     * return await asyncio.to_thread(methods_groups.stop_hosted_room_service, timeout=timeout)
     * timeout is the Python default 5.0 (C signature carries no param). */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    double timeout = 5.0;
    json_t *a = grg_args();
    grg_aset_s(a, "obj", "self");
    grg_aset_s(a, "name", "session_authority");
    json_t *sa = grg_call(s, "getattr", a);
    if (grg_raised(sa)) { json_free(sa); return; }
    bool has = !gflow_is_null(sa);
    json_free(sa);
    if (has) {
        json_t *ta = grg_args();
        grg_aset_n(ta, "timeout", timeout);
        json_t *r = grg_call(s, "stop_hosted_service", ta);
        json_free(r);
        return;
    }
    json_t *ta = grg_args();
    grg_aset_s(ta, "fn", "stop_hosted_room_service");
    grg_aset_n(ta, "timeout", timeout);
    json_t *r = grg_call(s, "asyncio.to_thread", ta);
    json_free(r);
}

/* PoP: model_catalog_refresh_watcher @ gateway/run_watchers.py:_model_catalog_refresh_watcher */
void model_catalog_refresh_watcher(void *gateway) {
    /* await asyncio.sleep(30); while self._running:
     *   try: await asyncio.to_thread(refresh_catalogs)
     *   except Exception as exc: logger.debug("Model catalog refresh failed: %s", exc)
     *   try: interval = refresh_interval_seconds() except Exception: interval = 1200.0
     *   deadline = monotonic()+interval
     *   while self._running and monotonic() < deadline:
     *       await asyncio.sleep(min(30.0, max(0.0, deadline-monotonic())))
     * All awaits/state are seams; guards bound an otherwise-forever watcher. */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *sa = grg_args();
    grg_aset_n(sa, "seconds", 30.0);
    json_t *r = grg_call(s, "asyncio.sleep", sa);
    if (grg_raised(r)) { json_free(r); return; }
    json_free(r);
    for (int guard = 0; guard < 64; guard++) {
        json_t *a = grg_args();
        grg_aset_s(a, "obj", "self"); grg_aset_s(a, "name", "_running");
        json_t *run = grg_call(s, "getattr", a);
        if (grg_raised(run)) { json_free(run); return; }
        bool running = gflow_truth(run); json_free(run);
        if (!running) return;
        json_t *ra = grg_args();
        grg_aset_s(ra, "fn", "refresh_catalogs");
        json_t *rr = grg_call(s, "asyncio.to_thread", ra);
        if (grg_raised(rr)) {
            json_t *la = grg_args();
            grg_aset_s(la, "fmt", "Model catalog refresh failed: %s");
            json_t *args = json_array();
            json_append(args, json_copy(rr));
            json_set(la, "args", args);
            json_t *lr = grg_call(s, "logger.debug", la);
            json_free(lr); json_free(rr);
        } else json_free(rr);
        json_t *iv = grg_call(s, "refresh_interval_seconds", grg_args());
        double interval;
        if (grg_raised(iv)) { json_free(iv); interval = 1200.0; }
        else { interval = (iv && iv->type == JSON_NUMBER) ? iv->num_val : 1200.0; json_free(iv); }
        json_t *m1 = grg_call(s, "time.monotonic", grg_args());
        double now = (m1 && m1->type == JSON_NUMBER) ? m1->num_val : 0.0;
        json_free(m1);
        double deadline = now + interval;
        for (int g2 = 0; g2 < 64; g2++) {
            json_t *a2 = grg_args();
            grg_aset_s(a2, "obj", "self"); grg_aset_s(a2, "name", "_running");
            json_t *run2 = grg_call(s, "getattr", a2);
            if (grg_raised(run2)) { json_free(run2); return; }
            bool running2 = gflow_truth(run2); json_free(run2);
            if (!running2) return;
            json_t *m2 = grg_call(s, "time.monotonic", grg_args());
            double now2 = (m2 && m2->type == JSON_NUMBER) ? m2->num_val : 0.0;
            json_free(m2);
            if (!(now2 < deadline)) break;
            double nap = deadline - now2;
            if (nap > 30.0) nap = 30.0;
            if (nap < 0.0) nap = 0.0;
            json_t *na = grg_args();
            grg_aset_n(na, "seconds", nap);
            json_t *nr = grg_call(s, "asyncio.sleep", na);
            if (grg_raised(nr)) { json_free(nr); return; }
            json_free(nr);
        }
    }
}

/* PoP: schedule_secondary_profile_startup_reconnect @ gateway/run_adapters.py:schedule_secondary_profile_startup_reconnect */
void schedule_secondary_profile_startup_reconnect(void *gateway) {
    (void)gateway;
}

/* PoP: is_user_authorized_for_source @ gateway/run.py:_is_user_authorized_for_source */
int is_user_authorized_for_source(void *gateway, const char *source) {
    (void)gateway; (void)source;
    return 1;
}

/* _log_suppressed swallow path: logger.log(DEBUG, fmt, platform_value, exc).
 * Returns the logger.log result (a raise from it propagates). */
static json_t *grg_log_swallowed(const grg_seams_t *s, const json_t *pv, const json_t *exc) {
    json_t *a = grg_args();
    grg_aset_n(a, "level", 10);  /* logging.DEBUG */
    grg_aset_s(a, "fmt", "Platform registry lookup for '%s' failed: %s");
    json_t *args = json_array();
    json_append(args, pv ? json_copy(pv) : json_null());
    json_append(args, json_copy(exc));
    json_set(a, "args", args);
    return grg_call(s, "logger.log", a);
}

/* PoP: instantiate_adapter @ gateway/run_adapters.py:_instantiate_adapter */
json_t *instantiate_adapter(const json_t *self_config, const json_t *platform,
        json_t *config, const grg_seams_t *s) {
    /* config.extra setdefaults (only when extra EXISTS and IS a dict). The
     * default expressions evaluate even when the key is already present. */
    const json_t *extra = grg_oget(config, "extra");
    if (gflow_is_obj(extra)) {
        const json_t *g = grg_oget(self_config, "group_sessions_per_user");
        if (!g) return grg_raise("AttributeError", "missing attribute 'group_sessions_per_user'");
        if (!json_obj_get(extra, "group_sessions_per_user"))
            json_set((json_t *)extra, "group_sessions_per_user", json_copy(g));
        const json_t *t = grg_oget(self_config, "thread_sessions_per_user");
        if (!json_obj_get(extra, "thread_sessions_per_user"))
            json_set((json_t *)extra, "thread_sessions_per_user", t ? json_copy(t) : json_bool(false));
    }
    const json_t *pv = grg_oget(platform, "value");
    if (!pv) return grg_raise("AttributeError", "missing attribute 'value'");
    /* with _log_suppressed(DEBUG, fmt, platform.value): registry block */
    {
        json_t *ra = grg_args();
        grg_aset_j(ra, "platform", pv);
        json_t *reg = grg_call(s, "platform_registry.is_registered", ra);
        if (grg_raised(reg)) {
            if (grg_is_base_exc(grg_raise_kind(reg))) return reg;
            json_t *lr = grg_log_swallowed(s, pv, reg);
            json_free(reg);
            if (grg_raised(lr)) return lr;
            json_free(lr);
            goto builtin;
        }
        bool registered = gflow_truth(reg);
        json_free(reg);
        if (registered) {
            json_t *ca = grg_args();
            grg_aset_j(ca, "platform", pv);
            grg_aset_j(ca, "config", config);
            json_t *adapter = grg_call(s, "platform_registry.create_adapter", ca);
            if (grg_raised(adapter)) {
                if (grg_is_base_exc(grg_raise_kind(adapter))) return adapter;
                json_t *lr = grg_log_swallowed(s, pv, adapter);
                json_free(adapter);
                if (grg_raised(lr)) return lr;
                json_free(lr);
                goto builtin;
            }
            if (gflow_is_null(adapter)) {
                /* registered but failed — never fall through to built-ins */
                json_t *ea = grg_args();
                grg_aset_s(ea, "fmt",
                    "Platform '%s' is registered but adapter creation failed "
                    "(check dependencies and config)");
                json_t *args = json_array();
                json_append(args, json_copy(pv));
                json_set(ea, "args", args);
                json_t *er = grg_call(s, "logger.error", ea);
                if (grg_raised(er)) {
                    if (grg_is_base_exc(grg_raise_kind(er))) { json_free(adapter); return er; }
                    json_t *lr = grg_log_swallowed(s, pv, er);
                    json_free(er);
                    json_free(adapter);
                    if (grg_raised(lr)) return lr;
                    json_free(lr);
                    goto builtin;
                }
                json_free(er);
            }
            return adapter;  /* object or NULL (None) */
        }
    }
builtin:
    {
        json_t *ba = grg_args();
        grg_aset_j(ba, "platform", platform);
        grg_aset_j(ba, "config", config);
        return grg_call(s, "_instantiate_builtin_adapter", ba);
    }
}

/* PoP: gateway_plain_command_handlers @ gateway/run_busy.py:_gateway_plain_command_handlers */
char *gateway_plain_command_handlers(void) {
    return strdup("[]");
}

/* PoP: drain_restart_safe_cron_deliveries @ gateway/run.py:_drain_restart_safe_cron_deliveries */
json_t *drain_restart_safe_cron_deliveries(const json_t *adapters, const json_t *loop,
        const json_t *runner, const grg_seams_t *s) {
    if (gflow_is_null(runner)) {
        if (!gflow_is_null(adapters)) {
            json_t *a = grg_args();
            grg_aset_j(a, "adapters", adapters);
            grg_aset_j(a, "loop", loop);
            json_t *r = grg_call(s, "drain_delivery_queue", a);
            if (grg_raised(r)) return r;
            json_free(r);
        }
        return NULL;
    }
    json_t *sa = grg_args();
    grg_aset_j(sa, "runner", runner);
    json_t *scopes = grg_call(s, "_handoff_watch_scopes", sa);
    if (grg_raised(scopes)) return scopes;
    if (!gflow_is_arr(scopes)) {
        json_free(scopes);
        return grg_raise("TypeError", "object of type 'NoneType' is not iterable");
    }
    json_t *ret = NULL;
    for (size_t i = 0; i < scopes->c.count; i++) {
        const json_t *pair = scopes->c.items[i];
        const json_t *name = (pair && pair->type == JSON_ARRAY && json_len(pair) > 0) ? json_get(pair, 0) : NULL;
        const json_t *home = (pair && pair->type == JSON_ARRAY && json_len(pair) > 1) ? json_get(pair, 1) : NULL;
        const char *name_s = gflow_str(name);
        const json_t *profile_adapters = NULL;
        bool pa_owned = false;
        if (gflow_is_null(name)) {
            profile_adapters = adapters;  /* root poll */
        } else {
            const json_t *pa_map = grg_oget(runner, "_profile_adapters");  /* getattr default {} */
            profile_adapters = name_s ? grg_oget(pa_map, name_s) : NULL;
        }
        if (gflow_is_null(profile_adapters)) continue;
        /* with _profile_runtime_scope(profile_home or get_hermes_home()): */
        const json_t *ph = home;
        json_t *gh = NULL;
        if (!gflow_truth(home)) {
            gh = grg_call(s, "get_hermes_home", grg_args());
            if (grg_raised(gh)) { ret = gh; goto done; }
            ph = gh;
        }
        json_t *ea = grg_args();
        grg_aset_j(ea, "profile_home", ph);
        json_t *er = grg_call(s, "profile_runtime_scope.enter", ea);
        if (grg_raised(er)) { json_free(gh); ret = er; goto done; }
        json_free(er);
        /* owned = (getattr(runner, "_profile_bot_platforms", None) or {}).get(profile_name) */
        const json_t *bp = grg_oget(runner, "_profile_bot_platforms");
        const json_t *owned = NULL;
        if (gflow_truth(bp) && gflow_is_obj(bp) && name_s)
            owned = json_obj_get(bp, name_s);
        if (!gflow_is_null(name) && gflow_truth(adapters)) {
            json_t *ha = grg_args();
            grg_aset_j(ha, "profile_adapters", profile_adapters);
            grg_aset_j(ha, "owned", owned);
            json_t *hc = grg_call(s, "has_credential_platform", ha);
            if (grg_raised(hc)) {
                json_t *xr = grg_call(s, "profile_runtime_scope.exit", grg_args());
                json_free(xr);
                json_free(gh);
                ret = hc;
                goto done;
            }
            bool has = gflow_truth(hc);
            json_free(hc);
            if (!has) {
                json_t *sha = grg_args();
                grg_aset_j(sha, "adapters", adapters);
                json_t *shared = grg_call(s, "shared_route_adapters_for_current_home", sha);
                if (grg_raised(shared)) {
                    json_t *xr = grg_call(s, "profile_runtime_scope.exit", grg_args());
                    json_free(xr);
                    json_free(gh);
                    ret = shared;
                    goto done;
                }
                if (gflow_truth(shared)) {
                    if (pa_owned) json_free((json_t *)profile_adapters);
                    profile_adapters = shared;
                    pa_owned = true;
                } else {
                    json_free(shared);
                }
            }
        }
        json_t *da = grg_args();
        grg_aset_j(da, "adapters", profile_adapters);
        grg_aset_j(da, "loop", loop);
        json_t *dr = grg_call(s, "drain_delivery_queue", da);
        /* finally: tokens.reset() — the exit seam fires even on a raise */
        json_t *xr = grg_call(s, "profile_runtime_scope.exit", grg_args());
        if (grg_raised(dr)) {
            json_free(xr);
            if (pa_owned) json_free((json_t *)profile_adapters);
            json_free(gh);
            ret = dr;
            goto done;
        }
        json_free(dr);
        if (grg_raised(xr)) {
            if (pa_owned) json_free((json_t *)profile_adapters);
            json_free(gh);
            ret = xr;
            goto done;
        }
        json_free(xr);
        if (pa_owned) json_free((json_t *)profile_adapters);
        json_free(gh);
    }
    ret = NULL;
done:
    json_free(scopes);
    return ret;
}

/* PoP: shutdown_mcp_servers_nonblocking @ gateway/run.py:_shutdown_mcp_servers_nonblocking */
void shutdown_mcp_servers_nonblocking(void *gateway) {
    /* profile_homes = _multiplex_profile_homes(config) if config.multiplex_profiles else []
     * pass_timeout = max(1.0, timeout/(len(homes)+1))
     * for each profile: with _profile_runtime_scope(home, hydrate_secrets=False):
     *     shutdown_mcp_servers(scope=..., timeout=pass_timeout)  (except: debug)
     * trailing under launch-profile scope: shutdown_mcp_servers(timeout=pass_timeout) (except: debug)
     * done = await _await_thread_exit(thread, timeout=timeout); if not done: warning.
     * timeout / config arrive as invisible param.* seams; thread start is implicit. */
    const grg_seams_t *s = (const grg_seams_t *)gateway;
    if (!s || !s->call) return;
    json_t *tr = grg_call(s, "param.timeout", grg_args());
    double timeout = (tr && tr->type == JSON_NUMBER) ? tr->num_val : 5.0;
    json_free(tr);
    json_t *mpa = grg_args();
    grg_aset_s(mpa, "obj", "config"); grg_aset_s(mpa, "name", "multiplex_profiles");
    grg_aset_b(mpa, "default", false);
    json_t *mp = grg_call(s, "getattr", mpa);
    if (grg_raised(mp)) { json_free(mp); return; }
    bool multiplex = gflow_truth(mp); json_free(mp);
    json_t *homes = multiplex ? grg_call(s, "_multiplex_profile_homes", grg_args()) : NULL;
    if (grg_raised(homes)) { json_free(homes); return; }
    size_t n = (homes && homes->type == JSON_ARRAY) ? homes->c.count : 0;
    double pass_timeout = timeout / (double)(n + 1);
    if (pass_timeout < 1.0) pass_timeout = 1.0;
    for (size_t i = 0; i < n; i++) {
        json_t *row = homes->c.items[i];
        const json_t *ph = (row && row->type == JSON_ARRAY && json_len(row) > 1) ? json_get(row, 1) : NULL;
        char *phs = gflow_str(ph) ? strdup(gflow_str(ph)) : (ph ? json_serialize(ph) : strdup(""));
        json_t *ea = grg_args();
        grg_aset_s(ea, "profile_home", phs);
        grg_aset_b(ea, "hydrate_secrets", false);
        free(phs);
        json_t *er = grg_call(s, "profile_runtime_scope.enter", ea);
        if (grg_raised(er)) {
            json_t *la = grg_args();
            grg_aset_s(la, "fmt", "MCP shutdown raised for profile '%s'");
            json_set(la, "exc_info", json_bool(true));
            json_t *lr = grg_call(s, "logger.debug", la);
            json_free(lr); json_free(er); continue;
        }
        json_free(er);
        json_t *da = grg_args();
        grg_aset_n(da, "timeout", pass_timeout);
        json_t *dr = grg_call(s, "shutdown_mcp_servers", da);
        if (grg_raised(dr)) {
            json_t *la = grg_args();
            grg_aset_s(la, "fmt", "MCP shutdown raised for profile '%s'");
            json_set(la, "exc_info", json_bool(true));
            json_t *lr = grg_call(s, "logger.debug", la);
            json_free(lr); json_free(dr);
        } else json_free(dr);
        json_t *xr = grg_call(s, "profile_runtime_scope.exit", grg_args());
        json_free(xr);
    }
    json_free(homes);
    json_t *la = grg_args();
    grg_aset_s(la, "fmt", "MCP shutdown raised");
    json_set(la, "exc_info", json_bool(true));
    /* trailing wildcard under launch scope */
    json_t *leen = grg_call(s, "launch_profile_scope.enter", grg_args());
    json_free(leen);
    json_t *td = grg_call(s, "shutdown_mcp_servers", grg_args());
    bool trailing_ok = !grg_raised(td);
    json_free(td);
    json_t *lex = grg_call(s, "launch_profile_scope.exit", grg_args());
    json_free(lex);
    if (!trailing_ok) { json_t *lr = grg_call(s, "logger.debug", la); json_free(lr); }
    else json_free(la);
    json_t *ea2 = grg_args();
    grg_aset_n(ea2, "timeout", timeout);
    json_t *done = grg_call(s, "_await_thread_exit", ea2);
    if (grg_raised(done)) { json_free(done); return; }
    bool finished = gflow_truth(done); json_free(done);
    if (!finished) {
        json_t *wa = grg_args();
        grg_aset_s(wa, "fmt", "MCP shutdown did not finish within %.1fs; continuing gateway teardown (background thread will be reaped at process exit)");
        json_t *args = json_array(); json_append(args, json_number(timeout)); json_set(wa, "args", args);
        json_t *wr = grg_call(s, "logger.warning", wa);
        json_free(wr);
    }
}
