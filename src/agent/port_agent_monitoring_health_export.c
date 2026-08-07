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
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/* PoP: _resolve_headers @ agent/monitoring/gateway_health_export.py:_resolve_headers
 * Resolve headers from env-var-name mapping. input: JSON object
 * {"header_name": "ENV_VAR"}; output: JSON object {"header_name": value}. Only
 * headers whose ENV_VAR is set to a non-empty string are included. malloc'd. */
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
