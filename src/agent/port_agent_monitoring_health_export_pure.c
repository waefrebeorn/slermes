/*
 * port_agent_monitoring_health_export_pure.c — Pure helpers from
 * agent/monitoring/gateway_health_export.py that take plain data
 * (dicts / strings / env probes) and return plain strings/collections,
 * with no SDK wiring or I/O side effects.
 *
 * Ports:
 *   - _redact_string           (wraps redact_for_export, fail-open)
 *   - _safe_resource_attributes (allowlist + value validation)
 *   - _version                 (HERMES_VERSION string)
 *   - _profile                 (active profile name)
 *   - _install_id              (config.monitoring.install_id or "unknown")
 *   - _supervision_mode        (env probe)
 *   - _runtime_resource_attributes (compose _safe_resource_attributes + _install_id)
 *
 * Reuses:
 *   otlp_exporter_redact_for_export (from port_otlp_exporter.c)
 *   gw_safe_instance_id            (from port_gateway_health.c)
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_agent_monitoring_health_export_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

#include "libjson/json.h"
#include "port_otlp_exporter.h"
#include "hermes_gateway_health.h"
#include "port_agent_monitoring_health_export.h"

/* ── Resource attribute allowlist (mirrors _RESOURCE_ATTRIBUTE_KEYS) ── */
static const char *const RESOURCE_ATTR_KEYS[] = {
    "service.name",
    "service.namespace",
    "service.version",
    "service.instance.id",
    "deployment.environment.name",
    "cloud.provider",
    "cloud.platform",
    "cloud.region",
    "telemetry.scope",
    NULL,
};
static bool _is_resource_attr_key(const char *key)
{
    for (size_t i = 0; RESOURCE_ATTR_KEYS[i]; i++)
        if (strcmp(key, RESOURCE_ATTR_KEYS[i]) == 0) return true;
    return false;
}

/* _SAFE_RESOURCE_VALUE = re.compile(r"^[A-Za-z0-9._:/-]{1,128}$")
 * Use strspn instead of POSIX regex (avoid REG_ESPACE on joined patterns
 * per documented lesson #32 — though this is a single simple pattern,
 * strspn is still simpler and dependency-free for character-class matching). */
static bool _safe_resource_value(const char *text)
{
    if (!text || !*text) return false;
    size_t len = strlen(text);
    if (len < 1 || len > 128) return false;
    for (const char *p = text; *p; p++) {
        char c = *p;
        if (!(isalnum((unsigned char)c) || c == '.' || c == '_' ||
              c == ':' || c == '/' || c == '-'))
            return false;
    }
    return true;
}

/* ── _redact_string ──────────────────────────────────────────── */

/* PoP: _redact_string @ agent/monitoring/gateway_health_export.py:_redact_string */
/* redact_for_export(str(raw or ""), fail-open → "[redaction-unavailable]").
 * Truncated to limit (default 500). Returns malloc'd string. */
char *ghe_redact_string(const char *raw, int limit)
{
    if (limit <= 0) limit = 500;
    char *redacted = otlp_exporter_redact_for_export(raw ? raw : "");
    if (!redacted) {
        char *fallback = strdup("[redaction-unavailable]");
        return fallback; /* no truncation needed */
    }
    if ((int)strlen(redacted) > limit)
        redacted[limit] = '\0';
    return redacted;
}

/* ── _safe_resource_attributes ───────────────────────────────── */

/* PoP: _safe_resource_attributes @ agent/monitoring/gateway_health_export.py:_safe_resource_attributes */
/* Allowlist bounded resource labels and reject values changed by redaction.
 * raw_json: JSON object. Returns a malloc'd JSON object string. */
char *ghe_safe_resource_attributes(const char *raw_json)
{
    json_t *out = json_object();
    if (!raw_json) goto done;

    char *err = NULL;
    json_t *raw = json_parse(raw_json, &err);
    if (err) { free(err); }
    if (!raw || raw->type != JSON_OBJECT) {
        if (raw) json_free(raw);
        goto done;
    }

    /* Iterate object keys */
    if (raw && raw->type == JSON_OBJECT) {
        for (size_t ki = 0; ki < raw->c.count; ki++) {
            const char *key = raw->c.keys[ki];
            json_t *value = raw->c.items[ki];
            if (!_is_resource_attr_key(key)) continue;
            if (!value || value->type == JSON_NULL) continue;

            if (strcmp(key, "service.instance.id") == 0) {
                /* _safe_instance_id(value) */
                char *sval = value->type == JSON_STRING ? value->str_val : NULL;
                char *sid = gw_safe_instance_id(sval ? sval : "");
                if (sid) {
                    json_set(out, key, json_string(sid));
                    free(sid);
                }
                continue;
            }

            /* text = str(value) */
            char *text;
            if (value->type == JSON_STRING)
                text = strdup(value->str_val);
            else
                text = json_serialize(value); /* rough str() for non-strings */

            if (!text) continue;
            if (!_safe_resource_value(text)) { free(text); continue; }

            /* if _redact_string(text, limit=128) != text: continue */
            char *redacted = otlp_exporter_redact_for_export(text);
            bool skip = false;
            if (redacted) {
                if (strcmp(redacted, text) != 0) skip = true;
                free(redacted);
            }
            if (!skip) {
                json_set(out, key, json_string(text));
            }
            free(text);
        }
        json_free(raw);
    }

done:
    return json_serialize(out);
}

/* ── _version ────────────────────────────────────────────────── */

/* PoP: _version @ agent/monitoring/gateway_health_export.py:_version */
const char *ghe_version(void)
{
#ifdef HERMES_VERSION
    /* The C build macro is "<version>-slermes" but Python's
     * hermes_cli.__version__ is just "<version>". Strip the suffix for parity. */
    static char _buf[64];
    static bool _cached = false;
    if (!_cached) {
        const char *full = HERMES_VERSION;
        const char *dash = strstr(full, "-slermes");
        if (dash) {
            size_t n = (size_t)(dash - full);
            if (n >= sizeof(_buf)) n = sizeof(_buf) - 1;
            memcpy(_buf, full, n);
            _buf[n] = '\0';
        } else {
            snprintf(_buf, sizeof(_buf), "%s", full);
        }
        _cached = true;
    }
    return _buf;
#else
    return "unknown";
#endif
}

/* ── _profile ────────────────────────────────────────────────── */

/* PoP: _profile @ agent/monitoring/gateway_health_export.py:_profile */
/* Returns the active profile name, or "default".
 * Checks HERMESSES_PROFILE then HERMES_PROFILE env, falls back to "default". */
const char *ghe_profile(void)
{
    const char *p = getenv("HERMESSES_PROFILE");
    if (!p) p = getenv("HERMES_PROFILE");
    if (!p || !*p) return "default";
    return p;
}

/* ── _install_id ─────────────────────────────────────────────── */

/* PoP: _install_id @ agent/monitoring/gateway_health_export.py:_install_id */
/* config_json: JSON object, optionally with monitoring.install_id.
 * Returns the install_id string or "unknown" on failure. */
char *ghe_install_id(const char *config_json)
{
    if (!config_json) return strdup("unknown");

    char *err = NULL;
    json_t *cfg = json_parse(config_json, &err);
    if (err) { free(err); }
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return strdup("unknown");
    }
    json_t *mon = json_obj_get(cfg, "monitoring");
    if (mon && mon->type == JSON_OBJECT) {
        json_t *iid = json_obj_get(mon, "install_id");
        if (iid && iid->type == JSON_STRING && iid->str_val && *iid->str_val) {
            char *result = strdup(iid->str_val);
            json_free(cfg);
            return result;
        }
    }
    json_free(cfg);
    return strdup("unknown");
}

/* ── _supervision_mode ───────────────────────────────────────── */

/* PoP: _supervision_mode @ agent/monitoring/gateway_health_export.py:_supervision_mode */
/* Probe env vars to determine the supervision runtime. */
const char *ghe_supervision_mode(void)
{
    if (getenv("INVOCATION_ID")) return "systemd";
    if (getenv("S6_CMD_ARG0") || getenv("S6_VERSION")) return "s6";
    if (getenv("container") || (access("/.dockerenv", F_OK) == 0)) return "container";
    if (getenv("LAUNCHD_SOCKET")) return "launchd";
    return "manual";
}

/* ── _runtime_resource_attributes ──────────────────────────── */

/* PoP: _runtime_resource_attributes @ agent/monitoring/gateway_health_export.py:_runtime_resource_attributes */
/* Build the safe OTLP resource shared by metrics and diagnostic logs.
 * config_json: JSON config dict. telemetry_scope: the scope string.
 * Returns malloc'd JSON object string. */
char *ghe_runtime_resource_attributes(const char *config_json, const char *telemetry_scope)
{
    json_t *out = json_object();
    if (!telemetry_scope) telemetry_scope = "unknown";

    /* Build _gateway_health_config(config) then _safe_resource_attributes */
    /* _gateway_health_config is already ported — we call it */
    char *gh_cfg = he_gateway_health_config(config_json);
    char *safe = ghe_safe_resource_attributes(gh_cfg ? gh_cfg : "{}");
    free(gh_cfg);

    /* Merge safe attrs into out */
    if (safe) {
        char *err = NULL;
        json_t *safe_obj = json_parse(safe, &err);
        if (!err) free(err);
        if (safe_obj && safe_obj->type == JSON_OBJECT) {
            for (size_t ki = 0; ki < safe_obj->c.count; ki++) {
                const char *key = safe_obj->c.keys[ki];
                json_t *val = safe_obj->c.items[ki];
                json_set(out, key, json_copy(val));
            }
        }
        if (safe_obj) json_free(safe_obj);
        free(safe);
    }

    /* attrs["service.name"] = "hermes-gateway" (override) */
    json_set(out, "service.name", json_string("hermes-gateway"));

    /* attrs["service.instance.id"] = _safe_instance_id(_install_id(config)) */
    char *iid = ghe_install_id(config_json);
    char *sid = gw_safe_instance_id(iid ? iid : "unknown");
    if (sid) {
        json_set(out, "service.instance.id", json_string(sid));
        free(sid);
    }
    free(iid);

    /* attrs["telemetry.scope"] = telemetry_scope */
    json_set(out, "telemetry.scope", json_string(telemetry_scope));

    return json_serialize(out);
}
