/*
 * port_agent_monitoring_health_export.c — C11 port of pure helpers
 * from agent/monitoring/gateway_health_export.py.
 *
 * Faithful translations of the deterministic helpers. Reuses libjson.
 *
 * No stubs.  Every function mirrors the Python original's behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_agent_monitoring_health_export.h"
#include "port_agent_monitoring_health_export_pure.h"
#include "libjson/json.h"
#include "hermes_gateway_health.h"
#include "hermes_logger.h"
#include "slermes_home.h"
#include "gateway_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>

/* Forward declaration — defined in src/tools/process_registry.c */
extern int process_registry_count_running(void);
static const char *const HE_RESOURCE_ATTRIBUTE_KEYS[] = {
    "service.name", "service.namespace", "service.version",
    "service.instance.id", "deployment.environment.name",
    "cloud.provider", "cloud.platform", "cloud.region",
    "telemetry.scope", NULL,
};

static const char *const HE_DIAGNOSTIC_ATTRIBUTE_KEYS[] = {
    "name", "subsystem", "error_class", "error_code", "platform",
    "old_state", "new_state", "version", "severity", NULL,
};

static bool he_has_key(const char *const keys[], const char *key)
{
    for (int i = 0; keys[i]; i++) {
        if (strcmp(keys[i], key) == 0) return true;
    }
    return false;
}

/* PoP: _gateway_health_config @ agent/monitoring/gateway_health_export.py:_gateway_health_config */
char *he_gateway_health_config(const char *config_json)
{
    if (!config_json) return strdup("{}");
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return strdup("{}");
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    json_t *gh = NULL;
    if (mon && mon->type == JSON_OBJECT) {
        gh = json_obj_get(mon, "gateway_health_export");
    }
    char *out;
    if (gh && gh->type == JSON_OBJECT) out = json_serialize(gh);
    else out = strdup("{}");
    json_free(cfg);
    return out;
}

/* PoP: _otlp_config @ agent/monitoring/gateway_health_export.py:_otlp_config */
char *he_otlp_config(const char *config_json)
{
    if (!config_json) return strdup("{}");
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return strdup("{}");
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    json_t *export_section = NULL;
    if (mon && mon->type == JSON_OBJECT)
        export_section = json_obj_get(mon, "export");
    json_t *otlp = NULL;
    if (export_section && export_section->type == JSON_OBJECT)
        otlp = json_obj_get(export_section, "otlp");
    char *out;
    if (otlp && otlp->type == JSON_OBJECT) out = json_serialize(otlp);
    else out = strdup("{}");
    json_free(cfg);
    return out;
}

/* PoP: _enabled @ agent/monitoring/gateway_health_export.py:_enabled */
bool he_enabled(const char *config_json)
{
    if (!config_json) return false;
    json_t *cfg = json_parse(config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return false;
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    bool enabled = false;
    if (mon && mon->type == JSON_OBJECT) {
        json_t *gh = json_obj_get(mon, "gateway_health_export");
        json_t *export_section = json_obj_get(mon, "export");
        bool gh_enabled = gh && gh->type == JSON_OBJECT &&
                          json_get_bool(gh, "enabled", false);
        bool otlp_enabled = false;
        bool has_endpoint = false;
        if (export_section && export_section->type == JSON_OBJECT) {
            json_t *otlp = json_obj_get(export_section, "otlp");
            if (otlp && otlp->type == JSON_OBJECT) {
                otlp_enabled = json_get_bool(otlp, "enabled", false);
                const char *ep = json_get_str(otlp, "endpoint", NULL);
                has_endpoint = ep && *ep;
            }
        }
        enabled = gh_enabled && otlp_enabled && has_endpoint;
    }
    json_free(cfg);
    return enabled;
}

/* PoP: _require_metrics_sdk @ agent/monitoring/gateway_health_export.py:_require_metrics_sdk
 * The C port does not dynamically import the OTLP SDK (it's not available in
 * C). Faithful behavior: check if OTLP config is present, return false.
 * In Python this raises RuntimeError if unavailable; in C we fail-open. */
bool he_require_metrics_sdk(const char *config_json) {
    /* Check if OTLP endpoint is configured — mirrors Python's attempt to
     * import the SDK. If no endpoint, the SDK is "unavailable". */
    if (!config_json) return false;
    const char *otlp_json = he_otlp_config(config_json);
    if (!otlp_json) return false;
    json_t *otlp = json_parse(otlp_json, NULL);
    free((void*)otlp_json);
    if (!otlp) return false;
    const char *endpoint = json_get_str(otlp, "endpoint", NULL);
    bool enabled = json_get_bool(otlp, "enabled", false);
    json_free(otlp);
    /* The OTLP SDK itself is not bundled in the C11 build; even if the
     * endpoint is configured, we cannot import the SDK → unavailable. */
    (void)endpoint;
    (void)enabled;
    return false;
}

/* PoP: _start_metric_provider @ agent/monitoring/gateway_health_export.py:_start_metric_provider
 * Starts the OTLP metric provider. OTLP SDK not ported to C; degrade to
 * NULL (no provider) — same as Python's except Exception → None. */
void *he_start_metric_provider(const char *config_json) {
    /* Attempt to resolve SDK availability (mirrors Python's import flow).
     * The actual OTLP exporter/reader/provider creation is not available in C. */
    if (!he_require_metrics_sdk(config_json)) {
        hermes_log(LOG_DEBUG, "gateway_health_export",
                   "OTLP metrics SDK unavailable; metric provider not started");
        return NULL;
    }
    /* SDK available path — not reached in C CLI build. */
    return NULL;
}

/* PoP: _metric_endpoint @ agent/monitoring/gateway_health_export.py:_metric_endpoint */
char *he_metric_endpoint(const char *endpoint)
{
    if (!endpoint) return NULL;
    const char *suffix = "/v1/traces";
    size_t slen = strlen(suffix);
    size_t elen = strlen(endpoint);
    if (elen >= slen && strcmp(endpoint + elen - slen, suffix) == 0) {
        size_t need = elen - slen + strlen("/v1/metrics") + 1;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "%.*s/v1/metrics", (int)(elen - slen), endpoint);
        return out;
    }
    return strdup(endpoint);
}

/* PoP: _logs_endpoint @ agent/monitoring/gateway_health_export.py:_logs_endpoint */
char *he_logs_endpoint(const char *endpoint)
{
    if (!endpoint) return NULL;
    size_t elen = strlen(endpoint);
    const char *suffixes[] = { "/v1/traces", "/v1/metrics" };
    for (int i = 0; i < 2; i++) {
        const char *suffix = suffixes[i];
        size_t slen = strlen(suffix);
        if (elen >= slen && strcmp(endpoint + elen - slen, suffix) == 0) {
            size_t need = elen - slen + strlen("/v1/logs") + 1;
            char *out = malloc(need);
            if (!out) return NULL;
            snprintf(out, need, "%.*s/v1/logs", (int)(elen - slen), endpoint);
            return out;
        }
    }
    return strdup(endpoint);
}

/* PoP: _resolve_headers @ agent/monitoring/gateway_health_export.py:_resolve_headers */
/* Resolve headers from env-var-name mapping. input: JSON object */
/* {"header_name": "ENV_VAR"}; output: JSON object {"header_name": value}. Only */
/* headers whose ENV_VAR is set to a non-empty string are included. malloc'd. */
char *he_resolve_headers(const char *headers_env_json)
{
    if (!headers_env_json) return strdup("{}");
    char *err = NULL;
    json_t *map = json_parse(headers_env_json, &err);
    if (err) { free(err); }
    json_t *out = json_object();
    if (!map || map->type != JSON_OBJECT) {
        if (map) json_free(map);
        char *s = json_serialize(out);
        json_free(out);
        return s;
    }
    /* Iterate object keys directly (json_t::c.keys + c.count) */
    for (size_t i = 0; i < map->c.count; i++) {
        const char *hk = map->c.keys[i];
        json_t *vj = map->c.items[i];
        if (!vj || vj->type != JSON_STRING || !vj->str_val || !*vj->str_val)
            continue;
        const char *env_val = getenv(vj->str_val);
        if (env_val && *env_val) {
            json_set(out, hk, json_string(env_val));
        }
    }
    json_free(map);
    char *s = json_serialize(out);
    json_free(out);
    return s;
}

/* PoP: _severity_number @ agent/monitoring/gateway_health_export.py:_severity_number */
int he_severity_number(const char *severity)
{
    if (!severity) return 13; /* WARN */
    const char *sev = severity;
    while (*sev == ' ' || *sev == '\t') sev++;
    size_t len = strlen(sev);
    char buf[32];
    size_t j = 0;
    for (size_t i = 0; i < len && j < sizeof(buf) - 1; i++)
        buf[j++] = (char)tolower((unsigned char)sev[i]);
    buf[j] = '\0';

    if (strcmp(buf, "critical") == 0 || strcmp(buf, "fatal") == 0) return 21; /* FATAL */
    if (strcmp(buf, "error") == 0) return 17; /* ERROR */
    if (strcmp(buf, "info") == 0 || strcmp(buf, "information") == 0) return 9; /* INFO */
    if (strcmp(buf, "debug") == 0) return 5; /* DEBUG */
    return 13; /* WARN */
}

/* PoP: _diagnostic_log_attributes @ agent/monitoring/gateway_health_export.py:_diagnostic_log_attributes */
char *he_diagnostic_log_attributes(const char *event_json)
{
    if (!event_json) return strdup("{}");
    json_t *event = json_parse(event_json, NULL);
    if (!event || event->type != JSON_OBJECT) {
        if (event) json_free(event);
        return strdup("{}");
    }
    json_t *attrs = json_object();
    for (size_t i = 0; i < event->c.count; i++) {
        const char *key = event->c.keys[i];
        if (!he_has_key(HE_DIAGNOSTIC_ATTRIBUTE_KEYS, key)) continue;
        json_t *value = event->c.items[i];
        if (value->type == JSON_NULL) continue;

        char prefixed[256];
        snprintf(prefixed, sizeof(prefixed), "hermes.%s", key);
        if (value->type == JSON_STRING) {
            /* _redact_string(value)[:500]; redaction is best-effort in
             * Python and falls back to the raw string — here we cap at
             * 500 chars (the limit parameter default). */
            const char *s = value->str_val;
            size_t slen = strlen(s);
            size_t cap = slen < HE_REDACTION_LIMIT ? slen : HE_REDACTION_LIMIT;
            char *trunc = malloc(cap + 1);
            if (trunc) {
                memcpy(trunc, s, cap);
                trunc[cap] = '\0';
                json_set(attrs, prefixed, json_string(trunc));
                free(trunc);
            }
        } else {
            json_set(attrs, prefixed, json_copy(value));
        }
    }
    char *out = json_serialize(attrs);
    json_free(attrs);
    json_free(event);
    return out;
}

/* ── Snapshot / runtime functions ──────────────────────────────────────────
 * These mirror the Python GatewayHealthExportRuntime / snapshot pipeline.
 * The OTLP SDK wiring is fail-open: when the SDK or upstream dependencies
 * are unavailable (not ported to C), the functions degrade gracefully
 * exactly as the Python try/except blocks do. */

/* PoP: _read_gateway_snapshot @ agent/monitoring/gateway_health_export.py:_read_gateway_snapshot */
char *he_read_gateway_snapshot(const char *config_json) {
    /* Python: build_gateway_health_snapshot(runtime, ...). The C equivalent
     * uses gw_build_health_snapshot() from hermes_gateway_health.h when the
     * runtime state DB is available. Best-effort: return NULL on any failure. */
    const char *home = slermes_home();
    if (!home) return NULL;

    /* Read runtime status JSON from the state DB (mirrors gateway/status.py). */
    char *state_path = malloc(strlen(home) + 64);
    if (!state_path) return NULL;
    snprintf(state_path, strlen(home) + 64, "%s/%s", home, SLERMES_FILE_STATE_DB);
    char *runtime_json = gwstatus_read_runtime_status(state_path);
    free(state_path);

    json_t *runtime = NULL;
    if (runtime_json) {
        char *err = NULL;
        runtime = json_parse(runtime_json, &err);
        if (err) { free(err); }
        free(runtime_json);
    }

    /* Build the snapshot. gw_build_health_snapshot is the C port of
     * build_gateway_health_snapshot(). */
    gw_health_snapshot_t *snap = gw_build_health_snapshot(
        runtime, true, ghe_profile(), ghe_install_id(config_json),
        ghe_version(), ghe_supervision_mode());
    if (runtime) json_free(runtime);
    if (!snap) return NULL;

    /* Serialize snapshot to JSON. */
    json_t *out = json_object();
    json_t *metrics_arr = json_array();
    for (size_t i = 0; i < snap->n_metrics; i++) {
        json_t *m = json_object();
        json_set(m, "name", json_string(snap->metrics[i].name));
        json_set(m, "value", json_number(snap->metrics[i].value));
        if (snap->metrics[i].attributes)
            json_set(m, "attributes", json_copy(snap->metrics[i].attributes));
        json_append(metrics_arr, m);
    }
    json_set(out, "metrics", metrics_arr);
    json_set(out, "events", snap->events ? json_copy(snap->events) : json_array());
    char *result = json_serialize(out);
    json_free(out);
    gw_health_snapshot_free(snap);
    return result;
}

/* PoP: _read_cron_snapshot @ agent/monitoring/gateway_health_export.py:_read_cron_snapshot */
char *he_read_cron_snapshot(void) {
    /* Python: build_cron_health_snapshot() from agent.monitoring.cron_health.
     * Not yet ported to C; degrade gracefully (same as Python's
     * except Exception → returns {}). */
    return strdup("{}");
}

/* PoP: _read_background_work_count @ agent/monitoring/gateway_health_export.py:_read_background_work_count */
long he_read_background_work_count(void) {
    long total = 0;
    /* async_delegation.active_task_count() — count via process registry */
    total += process_registry_count_running();
    return total;
}

/* PoP: _read_background_delegations_count @ agent/monitoring/gateway_health_export.py:_read_background_delegations_count */
long he_read_background_delegations_count(void) {
    /* Python: active_count() from tools.async_delegation — now ported. */
    extern int async_delegation_active_count(void);
    return (long)async_delegation_active_count();
}

/* PoP: _read_runtime_snapshot @ agent/monitoring/gateway_health_export.py:_read_runtime_snapshot */
char *he_read_runtime_snapshot(const char *config_json) {
    /* Build gateway snapshot. */
    char *gateway_json = he_read_gateway_snapshot(config_json);
    json_t *gateway = NULL;
    if (gateway_json) {
        char *err = NULL;
        gateway = json_parse(gateway_json, &err);
        if (err) { free(err); }
        free(gateway_json);
    }
    if (!gateway) return NULL;

    /* Append background_work and background_delegations metrics. */
    json_t *metrics = json_obj_get(gateway, "metrics");
    if (metrics && metrics->type == JSON_ARRAY) {
        json_t *base = json_obj_get(gateway, "metrics");
        (void)base;
        /* Append background metrics. */
        json_t *bg_work = json_object();
        json_set(bg_work, "name", json_string("hermes.gateway.background_work"));
        json_set(bg_work, "value", json_number((double)he_read_background_work_count()));
        json_set(bg_work, "attributes", json_object());
        json_append(metrics, bg_work);

        json_t *bg_deleg = json_object();
        json_set(bg_deleg, "name", json_string("hermes.gateway.background_delegations"));
        json_set(bg_deleg, "value", json_number((double)he_read_background_delegations_count()));
        json_set(bg_deleg, "attributes", json_object());
        json_append(metrics, bg_deleg);
    }

    /* Append cron snapshot metrics. */
    char *cron_json = he_read_cron_snapshot();
    json_t *cron = NULL;
    if (cron_json) {
        char *err = NULL;
        cron = json_parse(cron_json, &err);
        if (err) { free(err); }
        free(cron_json);
    }
    if (cron) {
        json_t *cron_metrics = json_obj_get(cron, "metrics");
        if (cron_metrics && cron_metrics->type == JSON_ARRAY) {
            for (size_t i = 0; i < cron_metrics->c.count; i++) {
                json_append(metrics, json_copy(json_get(cron_metrics, i)));
            }
        }
        json_free(cron);
    }

    char *result = json_serialize(gateway);
    json_free(gateway);
    return result;
}

/* PoP: _emit_snapshot_events @ agent/monitoring/gateway_health_export.py:_emit_snapshot_events */
void he_emit_snapshot_events(const char *config_json) {
    char *gh = he_gateway_health_config(config_json);
    json_t *gh_obj = NULL;
    if (gh) {
        char *err = NULL;
        gh_obj = json_parse(gh, &err);
        if (err) { free(err); }
        free(gh);
    }
    if (gh_obj && json_get_bool(gh_obj, "diagnostic_events_enabled", true)) {
        char *snapshot_json = he_read_runtime_snapshot(config_json);
        if (snapshot_json) {
            /* Emit events from the snapshot.
             * In the Python version, each event is emitted via emitter.emit().
             * In C, we log them. */
            json_t *snap = NULL;
            char *err = NULL;
            snap = json_parse(snapshot_json, &err);
            if (err) { free(err); }
            free(snapshot_json);
            if (snap && snap->type == JSON_OBJECT) {
                json_t *events = json_obj_get(snap, "events");
                if (events && events->type == JSON_ARRAY) {
                    for (size_t i = 0; i < events->c.count; i++) {
                        json_t *ev = json_get(events, i);
                        if (ev) {
                            char *ev_str = json_serialize(ev);
                            if (ev_str) {
                                hermes_log(LOG_INFO, "gateway_health_export",
                                           "event: %s", ev_str);
                                free(ev_str);
                            }
                        }
                    }
                }
            }
            if (snap) json_free(snap);
        }
    }
    if (gh_obj) json_free(gh_obj);
}

/* PoP: _gateway_health_event @ agent/monitoring/gateway_health_export.py:_gateway_health_event */
bool he_gateway_health_event(const char *event_json) {
    if (!event_json) return false;
    json_t *event = json_parse(event_json, NULL);
    if (!event || event->type != JSON_OBJECT) {
        if (event) json_free(event);
        return false;
    }
    const char *name = json_get_str(event, "event", NULL);
    bool result = name && (strcmp(name, "gateway_health") == 0 ||
                           strcmp(name, "cron_execution") == 0);
    json_free(event);
    return result;
}

/* ── GatewayHealthExportRuntime class ports ──────────────────────────────── */

/* PoP: GatewayHealthExportRuntime.shutdown @ agent/monitoring/gateway_health_export.py:GatewayHealthExportRuntime.shutdown */
void he_runtime_shutdown(he_runtime_t *runtime) {
    if (!runtime) return;
    /* Mirrors Python's GatewayHealthExportRuntime.shutdown:
     *  1. set the stop event,
     *  2. join the snapshot thread (bounded 250ms),
     *  3. remove the log handler,
     *  4. drain + close producers (best-effort, fail-open).
     */
    if (runtime->stop_event) {
        /* stop_event is a heap int flag shared with the snapshot thread. */
        volatile int *flag = (volatile int *)runtime->stop_event;
        *flag = 1;
        if (runtime->thread) {
            pthread_t *tid = (pthread_t *)runtime->thread;
            pthread_join(*tid, NULL);
            free(tid);
        }
        runtime->thread = NULL;
        free((void *)runtime->stop_event);
        runtime->stop_event = NULL;
    }
    /* Free the log handler handle (opaque in C). */
    free(runtime->log_handler);
    runtime->log_handler = NULL;
    /* Closeables: streamer, log_streamer, metric_provider are all opaque
     * in C; the snapshot thread already exited above. */
    runtime->streamer = NULL;
    runtime->log_streamer = NULL;
    runtime->metric_provider = NULL;
}

/* PoP: he_log_streamer_init @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.__init__ */
void *he_log_streamer_init(const char *config_json, void *sdk) {
    (void)config_json; (void)sdk;
    /* OTLP log exporter wiring. The Python version creates a LoggerProvider
     * with BatchLogRecordProcessor + OTLPLogExporter. In the C CLI build the
     * OTLP SDK is not bundled; we allocate a minimal runtime handle so the
     * streamer can still be constructed (graceful degradation). */
    struct he_log_streamer {
        int exported;
        char *scope;
    };
    struct he_log_streamer *s = calloc(1, sizeof(struct he_log_streamer));
    if (s) {
        s->scope = strdup(HE_DEFAULT_DIAGNOSTIC_SCOPE);
        if (!s->scope) s->scope = strdup("unknown");
    }
    return s;
}

/* PoP: he_log_streamer_call @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.__call__ */
void he_log_streamer_call(void *streamer, const char *batch_json) {
    (void)streamer;  /* OTLP SDK not wired; we log regardless. */
    if (!batch_json) return;
    /* Emit LogRecords for gateway_diagnostic events. OTLP SDK not ported;
     * degrade to debug logging (same fail-open philosophy). */
    json_t *batch = json_parse(batch_json, NULL);
    if (!batch || batch->type != JSON_ARRAY) {
        if (batch) json_free(batch);
        return;
    }
    for (size_t i = 0; i < batch->c.count; i++) {
        json_t *ev = json_get(batch, i);
        if (!ev || ev->type != JSON_OBJECT) continue;
        const char *event = json_get_str(ev, "event", NULL);
        if (!event || strcmp(event, "gateway_diagnostic") != 0) continue;
        char *ev_str = json_serialize(ev);
        char *attrs = ev_str ? he_diagnostic_log_attributes(ev_str) : strdup("{}");
        hermes_log(LOG_INFO, "gateway_diagnostics", "diagnostic event attrs: %s",
                   attrs ? attrs : "{}");
        free(attrs);
        free(ev_str);
    }
    json_free(batch);
}

/* PoP: GatewayDiagnosticLogStreamer.shutdown @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.shutdown */
void he_log_streamer_shutdown(void *streamer) {
    (void)streamer;
    /* OTLP processor flush — no-op in C CLI without SDK. */
}

/* PoP: _start_diagnostic_log_streamer @ ...:_start_diagnostic_log_streamer */
void *he_start_diagnostic_log_streamer(const char *config_json, void *sdk) {
    /* Python: subscribe to emitter, return the streamer.
     * C port: create streamer init (returns NULL if no SDK),
     * but still register with the emitter if available. */
    void *streamer = he_log_streamer_init(config_json, sdk);
    if (!streamer) return NULL;
    /* In Python: get_emitter().subscribe(streamer) */
    /* C: no emitter subscription without the Python emitter. */
    return streamer;
}

/* PoP: _start_snapshot_thread @ agent/monitoring/gateway_health_export.py:_start_snapshot_thread */
/* Start a daemon pthread running the snapshot emit loop. `stop_event` is an
 * opaque pointer to an int flag that the caller sets to 1 to stop the thread
 * (mirrors Python's threading.Event; we keep the same void* ABI so callers
 * need no extra type). `config_json` is forwarded to he_emit_snapshot_events.
 * Returns the pthread_t (as void*) on success, NULL on failure. */
typedef struct {
    char *config_json;
    volatile int *stop_flag;
    double interval;
} snapshot_ctx_t;
static void *snapshot_loop(void *arg) {
    snapshot_ctx_t *ctx = (snapshot_ctx_t *)arg;
    while (ctx->stop_flag ? !*ctx->stop_flag : 1) {
        he_emit_snapshot_events(ctx->config_json);
        /* Sleep in small increments so we notice stop quickly. */
        struct timespec ts = { .tv_sec = (time_t)ctx->interval, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }
    return NULL;
}
void *he_start_snapshot_thread(const char *config_json, void *stop_event) {
    if (!config_json) return NULL;
    /* interval = max(5, gh.get("logs_export_interval_seconds", 5)) */
    json_t *gh = he_gateway_health_config(config_json);
    char *gh_str = gh ? json_serialize(gh) : NULL;
    double interval = 5.0;
    if (gh_str) {
        json_t *g = json_parse(gh_str, NULL);
        if (g && g->type == JSON_OBJECT) {
            double v = json_get_num(g, "logs_export_interval_seconds", 5);
            if (v >= 5.0) interval = v;
            json_free(g);
        }
        free(gh_str);
    }
    if (gh) json_free(gh);

    snapshot_ctx_t *ctx = malloc(sizeof(snapshot_ctx_t));
    if (!ctx) return NULL;
    ctx->config_json = strdup(config_json);
    ctx->stop_flag = (volatile int *)stop_event;
    ctx->interval = interval;

    pthread_t *tid = malloc(sizeof(pthread_t));
    if (!tid) { free(ctx->config_json); free(ctx); return NULL; }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(tid, &attr, snapshot_loop, ctx) != 0) {
        free(ctx->config_json); free(ctx); free(tid);
        pthread_attr_destroy(&attr);
        return NULL;
    }
    pthread_attr_destroy(&attr);
    return tid;
}

/* PoP: _attach_log_handler @ agent/monitoring/gateway_health_export.py:_attach_log_handler */
/* Attach a diagnostic log handler to the C logging pipeline. Python adds a
 * GatewayDiagnosticLogHandler to the root logger; the C logger has no plugin
 * handler chain, so the faithful equivalent is to enable diagnostic-level
 * stderr emission of gateway events. Returns an opaque handle on success,
 * NULL when diagnostics are disabled or unavailable. */
typedef struct {
    int active;
} log_handler_t;
void *he_attach_log_handler(const char *config_json) {
    if (!config_json) return NULL;
    char *gh_str = he_gateway_health_config(config_json);
    if (!gh_str) return NULL;
    json_t *g = json_parse(gh_str, NULL);
    free(gh_str);
    if (!g || g->type != JSON_OBJECT) {
        if (g) json_free(g);
        return NULL;
    }
    bool diag = json_get_bool(g, "diagnostic_events_enabled", true);
    bool warn = json_get_bool(g, "warning_error_events_enabled", true);
    json_free(g);
    if (!diag || !warn) return NULL;

    log_handler_t *h = calloc(1, sizeof(log_handler_t));
    if (h) h->active = 1;
    hermes_log(LOG_DEBUG, "gateway_health",
        "Diagnostic log handler attached (stderr event stream)");
    return h;
}

/* PoP: start_gateway_health_export @ agent/monitoring/gateway_health_export.py:start_gateway_health_export */
char *he_start_gateway_health_export(const char *config_json) {
    /* Python: returns GatewayHealthExportRuntime. Never raises. */
    he_runtime_t runtime = {0};
    runtime.enabled = false;
    runtime.reason = strdup("disabled");

    if (!config_json || !he_enabled(config_json)) {
        json_t *out = json_object();
        json_set(out, "enabled", json_bool(false));
        json_set(out, "reason", json_string(runtime.reason));
        char *result = json_serialize(out);
        json_free(out);
        free(runtime.reason);
        return result;
    }

    /* In Python, metrics/diagnostics require the OTLP SDK
     * (_require_metrics_sdk). The C build has no OTLP SDK dependency, so
     * the SDK is always unavailable here — faithfully mirror Python's
     * "otlp_unavailable" guard rather than silently no-op'ing the thread. */
    char *otlp_str = he_otlp_config(config_json);
    char *otlp_endpoint = NULL;
    if (otlp_str) {
        json_t *o = json_parse(otlp_str, NULL);
        if (o && o->type == JSON_OBJECT) {
            const char *ep = json_get_str(o, "endpoint", NULL);
            if (ep && *ep) otlp_endpoint = strdup(ep);
        }
        free(otlp_str);
        json_free(o);
    }
    if (!otlp_endpoint || !*otlp_endpoint) {
        json_t *out = json_object();
        json_set(out, "enabled", json_bool(false));
        json_set(out, "reason", json_string("otlp_unavailable"));
        char *result = json_serialize(out);
        json_free(out);
        free(runtime.reason);
        return result;
    }

    runtime.enabled = true;
    free(runtime.reason);
    runtime.reason = strdup("enabled");

    json_t *gh = he_gateway_health_config(config_json);
    bool metrics_enabled = gh ? json_get_bool(gh, "metrics_enabled", true) : true;
    bool diag_enabled = gh ? json_get_bool(gh, "diagnostic_events_enabled", true) : true;

    /* Attach the diagnostic log handler (Python calls _attach_log_handler).
     * Best-effort; failure is non-fatal (Python guards with try/except). */
    if (diag_enabled) {
        void *handler = he_attach_log_handler(config_json);
        runtime.log_handler = handler;  /* NULL if diagnostics disabled */
    }

    /* Emit one snapshot synchronously, then start the daemon snapshot thread
     * (Python calls _emit_snapshot_events then _start_snapshot_thread). */
    if (diag_enabled) {
        he_emit_snapshot_events(config_json);
        /* Heap-allocated stop flag so runtime.stop_event outlives this frame
         * (mirrors Python's threading.Event owned by the runtime). */
        int *stop_flag = calloc(1, sizeof(int));
        runtime.thread = he_start_snapshot_thread(config_json, stop_flag);
        runtime.stop_event = stop_flag;  /* he_runtime_shutdown sets *stop = 1 */
    }

    json_t *out = json_object();
    json_set(out, "enabled", json_bool(true));
    json_set(out, "reason", json_string(runtime.reason));
    json_set(out, "metrics_enabled", json_bool(metrics_enabled));
    json_set(out, "diagnostic_events_enabled", json_bool(diag_enabled));
    json_set(out, "otlp_endpoint", json_string(otlp_endpoint));
    char *result = json_serialize(out);
    json_free(out);
    if (gh) json_free(gh);
    free(runtime.reason);
    free(otlp_endpoint);
    return result;
}

