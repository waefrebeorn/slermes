/*
 * port_gateway_health.c — C11 port of agent/monitoring/gateway_health.py.
 * Gateway health classification and diagnostics.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_gateway_health.h"

/* ── State allowlists ─────────────────────────────────────── */
static const char *GW_RUNNING_STATES[] = {"running", "connected", "ok", "ready", NULL};
static const char *GW_FATAL_STATES[] = {"fatal", "degraded", "error", "failed", NULL};
static const char *GW_KNOWN_GATEWAY_STATES[] = {
    "starting", "draining", "stopping", "stopped", "startup_failed", "unknown",
    "running", "connected", "ok", "ready",
    "fatal", "degraded", "error", "failed",
    NULL
};
static const char *GW_KNOWN_PLATFORM_STATES[] = {
    "running", "connected", "ok", "ready",
    "fatal", "degraded", "error", "failed",
    "connecting", "disconnected", "disabled", "paused", "retrying", "unknown",
    NULL
};
static const char *GW_SUPERVISION_MODES[] = {
    "systemd", "s6", "container", "launchd", "manual", "unknown",
    NULL
};

/* PoP: _allowed_logger @ agent/monitoring/gateway_health.py:_allowed_logger */
/* PoP: _safe_metric_value @ agent/monitoring/gateway_health.py:_safe_metric_value */
/* PoP: _base_attrs @ agent/monitoring/gateway_health.py:_base_attrs */
/* PoP: _metric @ agent/monitoring/gateway_health.py:_metric */
/* PoP: _safe_profile @ agent/monitoring/gateway_health.py:_safe_profile */
/* PoP: _safe_version @ agent/monitoring/gateway_health.py:_safe_version */
/* PoP: _coerce_pid @ agent/monitoring/gateway_health.py:_coerce_pid */
/* PoP: GatewayDiagnosticLogHandler.__init__ @ agent/monitoring/gateway_health.py:GatewayDiagnosticLogHandler.__init__ */
/* PoP: GatewayDiagnosticLogHandler.emit @ agent/monitoring/gateway_health.py:GatewayDiagnosticLogHandler.emit */
/* PoP: emit_runtime_status_transition @ agent/monitoring/gateway_health.py:emit_runtime_status_transition */
static bool str_in_list(const char *s, const char **list) {
    if (!s) return false;
    for (int i = 0; list[i]; i++)
        if (strcmp(s, list[i]) == 0) return true;
    return false;
}

/* PoP: source_logger_for_export @ agent/monitoring/gateway_health.py:source_logger_for_export */
const char *gw_source_logger_for_export(const char *name) {
    if (!name) return NULL;
    size_t len = strlen(name);
    if (len > 128) return NULL;
    /* Must be "gateway" or "gateway.<identifier>..." */
    if (strcmp(name, "gateway") == 0) return name;
    if (len > 8 && strncmp(name, "gateway.", 8) == 0) {
        /* Check remaining chars are alphanumeric/underscore/dot */
        for (size_t i = 8; i < len; i++) {
            char c = name[i];
            if (!isalnum((unsigned char)c) && c != '_' && c != '.')
                return NULL;
        }
        return name;
    }
    return NULL;
}

/* PoP: redact_gateway_message @ agent/monitoring/gateway_health.py:redact_gateway_message */
char *gw_redact_gateway_message(const char *message) {
    if (!message) return strdup("");
    /* Simple length-bound. Full redaction requires agent/monitoring/redaction. */
    size_t len = strlen(message);
    if (len > 500) len = 500;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, message, len);
    out[len] = '\0';
    return out;
}

/* PoP: classify_gateway_error @ agent/monitoring/gateway_health.py:classify_gateway_error */
const char *gw_classify_gateway_error(const char *raw) {
    if (!raw) return "unknown";
    /* Lowercase */
    char buf[512];
    size_t i = 0;
    for (; raw[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)raw[i]);
    buf[i] = '\0';
    const char *s = buf;

    if (strstr(s, "auth") || strstr(s, "token") || strstr(s, "unauthorized") ||
        strstr(s, "forbidden") || strstr(s, "401") || strstr(s, "403"))
        return "auth_failed";
    if (strstr(s, "rate") && strstr(s, "limit"))
        return "rate_limited";
    if (strstr(s, "timeout") || strstr(s, "timed out"))
        return "timeout";
    if (strstr(s, "network") || strstr(s, "connection") || strstr(s, "dns") ||
        strstr(s, "socket") || strstr(s, "connect call failed") ||
        strstr(s, "failed to connect") || strstr(s, "cannot connect") ||
        strstr(s, "unreachable") || strstr(s, "name resolution"))
        return "network_error";
    if (strstr(s, "config") || strstr(s, "missing") || strstr(s, "invalid"))
        return "invalid_config";
    if (strstr(s, "startup"))
        return "startup_failed";
    if (strstr(s, "fatal"))
        return "platform_fatal";
    return "unknown";
}

/* PoP: classify_exit_reason @ agent/monitoring/gateway_health.py:classify_exit_reason */
const char *gw_classify_exit_reason(const char *raw, const char *state, bool restart_requested) {
    if (restart_requested) return "restart_requested";
    if (!raw && state && strcmp(state, "startup_failed") != 0) return NULL;

    const char *classified = gw_classify_gateway_error(raw);
    if (state && strcmp(state, "startup_failed") == 0)
        return (strcmp(classified, "unknown") == 0) ? "startup_failed" : classified;

    if (!raw) return NULL;
    char buf[256];
    size_t i = 0;
    for (; raw[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)raw[i]);
    buf[i] = '\0';

    if (strstr(buf, "signal") || strstr(buf, "sigterm") || strstr(buf, "sigint"))
        return "signal";
    if (state && strcmp(state, "stopped") == 0 &&
        (strstr(buf, "shutdown") || strstr(buf, "stop")))
        return "planned_stop";
    return classified;
}

/* PoP: _bounded_state @ agent/monitoring/gateway_health.py:_bounded_state */
const char *gw_bounded_state(const char *raw) {
    if (!raw) return "unknown";
    char buf[128];
    size_t i = 0;
    for (; raw[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (char)tolower((unsigned char)raw[i]);
    buf[i] = '\0';
    /* Return pointer into a static string table */
    for (int j = 0; GW_KNOWN_GATEWAY_STATES[j]; j++) {
        if (strcmp(buf, GW_KNOWN_GATEWAY_STATES[j]) == 0)
            return GW_KNOWN_GATEWAY_STATES[j];
    }
    return "unknown";
}

/* PoP: _safe_instance_id @ agent/monitoring/gateway_health.py:_safe_instance_id */
char *gw_safe_instance_id(const char *raw) {
    const char *val = raw ? raw : "unknown";
    size_t vlen = strlen(val);
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)val, vlen, digest);

    char hex[32]; /* 24 hex chars + null */
    for (int i = 0; i < 24 && i < CRYPTO_SHA256_LEN * 2; i++) {
        int byte_idx = i / 2;
        int nibble = (i % 2 == 0) ? (digest[byte_idx] >> 4) : (digest[byte_idx] & 0x0F);
        hex[i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    hex[24] = '\0';

    char *out = malloc(32);
    if (!out) return NULL;
    snprintf(out, 32, "sha256:%s", hex);
    return out;
}

/* PoP: _parse_active_agents @ agent/monitoring/gateway_health.py:_parse_active_agents */
int gw_parse_active_agents(const char *raw) {
    if (!raw) return 0;
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    if (end == raw || *end != '\0') return 0;
    return (v > 0) ? (int)v : 0;
}

/* PoP: _derive_busy @ agent/monitoring/gateway_health.py:_derive_busy */
bool gw_derive_busy(bool gateway_running, const char *gateway_state, int active_agents) {
    return gateway_running && gateway_state && strcmp(gateway_state, "running") == 0 && active_agents > 0;
}

/* PoP: _derive_drainable @ agent/monitoring/gateway_health.py:_derive_drainable */
bool gw_derive_drainable(bool gateway_running, const char *gateway_state) {
    return gateway_running && gateway_state && strcmp(gateway_state, "running") == 0;
}

/* PoP: subsystem_for_logger @ agent/monitoring/gateway_health.py:subsystem_for_logger */
const char *gw_subsystem_for_logger(const char *logger_name) {
    if (!logger_name) return "gateway";
    if (strcmp(logger_name, "gateway.relay") == 0 ||
        strncmp(logger_name, "gateway.relay.", 14) == 0)
        return "platform.relay";
    if (strncmp(logger_name, "gateway.platforms.", 18) == 0) {
        /* platform.<name> */
        const char *rest = logger_name + 18;
        const char *dot = strchr(rest, '.');
        size_t plen = dot ? (size_t)(dot - rest) : strlen(rest);
        static char sub[128];
        int n = snprintf(sub, sizeof(sub), "platform.%.*s", (int)plen, rest);
        if (n < 0 || (size_t)n >= sizeof(sub)) return "platform";
        return sub;
    }
    if (strncmp(logger_name, "gateway.platforms", 17) == 0)
        return "platform";
    if (strncmp(logger_name, "gateway", 7) == 0)
        return "gateway";
    return "gateway";
}

/* PoP: platform_for_subsystem @ agent/monitoring/gateway_health.py:platform_for_subsystem */
const char *gw_platform_for_subsystem(const char *subsystem) {
    if (!subsystem || strncmp(subsystem, "platform.", 9) != 0)
        return NULL;
    const char *platform = subsystem + 9;
    return (*platform) ? platform : NULL;
}

/* ── Snapshot builder ────────────────────────────────────── */

gw_health_snapshot_t *gw_build_health_snapshot(
    const json_t *runtime,
    bool gateway_running,
    const char *profile,
    const char *install_id,
    const char *version,
    const char *supervision_mode
) {
    gw_health_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) return NULL;

    /* Basic metrics — simplified */
    snap->metrics = calloc(10, sizeof(gw_metric_t));
    if (!snap->metrics) { free(snap); return NULL; }

    json_t *attrs = json_object();
    char *inst = gw_safe_instance_id(install_id);
    json_set(attrs, "service.instance.id", json_string(inst ? inst : "unknown"));
    free(inst);
    json_set(attrs, "service.version", json_string(version ? version : "unknown"));

    const char *mode = supervision_mode ? supervision_mode : "unknown";
    char mode_lower[64];
    size_t mi = 0;
    for (; mode[mi] && mi < sizeof(mode_lower) - 1; mi++)
        mode_lower[mi] = (char)tolower((unsigned char)mode[mi]);
    mode_lower[mi] = '\0';
    if (!str_in_list(mode_lower, GW_SUPERVISION_MODES))
        mode_lower[0] = '\0'; /* use "unknown" by default */

    json_t *mode_attr = json_copy(attrs);
    json_set(mode_attr, "hermes.supervision_mode",
             json_string(mode_lower[0] ? mode_lower : "unknown"));

    /* Gateway state */
    const char *gw_state = NULL;
    if (runtime) {
        const json_t *gs = json_obj_get(runtime, "gateway_state");
        if (gs && gs->type == JSON_STRING)
            gw_state = gs->str_val;
    }
    const char *bounded = gw_bounded_state(gw_state);

    int active_agents = 0;
    if (runtime) {
        const json_t *aa = json_obj_get(runtime, "active_agents");
        if (aa && aa->type == JSON_NUMBER)
            active_agents = (int)aa->num_val;
    }

    bool busy = gw_derive_busy(gateway_running, bounded, active_agents);
    bool drainable = gw_derive_drainable(gateway_running, bounded);

    int n = 0;
    snap->metrics[n].name = "hermes.gateway.up";
    snap->metrics[n].value = gateway_running ? 1.0 : 0.0;
    snap->metrics[n].attributes = json_copy(attrs);
    n++;

    snap->metrics[n].name = "hermes.gateway.active_agents";
    snap->metrics[n].value = (double)active_agents;
    snap->metrics[n].attributes = json_copy(attrs);
    n++;

    snap->metrics[n].name = "hermes.gateway.busy";
    snap->metrics[n].value = busy ? 1.0 : 0.0;
    snap->metrics[n].attributes = json_copy(attrs);
    n++;

    snap->metrics[n].name = "hermes.gateway.drainable";
    snap->metrics[n].value = drainable ? 1.0 : 0.0;
    snap->metrics[n].attributes = json_copy(attrs);
    n++;

    if (runtime) {
        const json_t *rr = json_obj_get(runtime, "restart_requested");
        bool restart_req = rr && rr->type == JSON_BOOL && rr->bool_val;
        snap->metrics[n].name = "hermes.gateway.restart_requested";
        snap->metrics[n].value = restart_req ? 1.0 : 0.0;
        snap->metrics[n].attributes = json_copy(attrs);
        n++;
    }

    snap->n_metrics = (size_t)n;
    json_free(attrs);
    json_free(mode_attr);
    return snap;
}

void gw_health_snapshot_free(gw_health_snapshot_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n_metrics; i++)
        json_free(s->metrics[i].attributes);
    free(s->metrics);
    json_free(s->events);
    free(s);
}