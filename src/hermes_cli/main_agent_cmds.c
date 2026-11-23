/*
 * main_agent_cmds.c — C port of upstream Hermes hermes_cli/main_agent_cmds.py.
 *
 * PoP: w13_mainagent_07d4b1_dict_or_empty @ hermes_cli/main_agent_cmds.py:_dict_or_empty
 * PoP: w13_mainagent_07d4b1_acp_flags @ hermes_cli/main_agent_cmds.py:cmd_acp (_ACP_FLAGS)
 * PoP: w13_mainagent_07d4b1_acp_env @ hermes_cli/main_agent_cmds.py:cmd_acp (env writes)
 * PoP: w13_mainagent_07d4b1_monitoring_lines @ hermes_cli/main_agent_cmds.py:cmd_monitoring
 *
 * Pure command-shape seams; no filesystem or process side effects.
 */
#include "noc_w13_mainagent_07d4b1.h"
#include "hermes_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static void py_scalar(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); break;
        case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, n, "%lld", (long long)v->num_val);
            else
                snprintf(buf, n, "%g", v->num_val);
            break;
        default: if (n) buf[0] = '\0'; break;
    }
}

json_t *w13_mainagent_07d4b1_dict_or_empty(const struct json_t *value) {
    if (value && value->type == JSON_OBJECT) return json_copy(value);
    return json_object();
}

static const char *const ACP_FLAGS[][2] = {
    {"acp_version", "--version"},
    {"check", "--check"},
    {"setup", "--setup"},
    {"setup_browser", "--setup-browser"},
    {"assume_yes", "--yes"},
    {NULL, NULL}
};

json_t *w13_mainagent_07d4b1_acp_flags(const struct json_t *args) {
    json_t *out = json_array();
    for (size_t i = 0; ACP_FLAGS[i][0]; i++)
        if (truthy(json_obj_get((json_t *)args, ACP_FLAGS[i][0])))
            json_append(out, json_string(ACP_FLAGS[i][1]));
    return out;
}

json_t *w13_mainagent_07d4b1_acp_env(const struct json_t *args) {
    json_t *out = json_object();
    static const char *const onoff[][2] = {
        {"accept_hooks", "HERMES_ACCEPT_HOOKS"},
        {"ignore_rules", "HERMES_IGNORE_RULES"},
        {NULL, NULL}
    };
    static const char *const strings[][2] = {
        {"model", "HERMES_MODEL"},
        {"provider", "HERMES_INFERENCE_PROVIDER"},
        {NULL, NULL}
    };
    for (size_t i = 0; onoff[i][0]; i++)
        if (truthy(json_obj_get((json_t *)args, onoff[i][0])))
            json_set(out, onoff[i][1], json_string("1"));
    for (size_t i = 0; strings[i][0]; i++) {
        const char *v = jstr(json_obj_get((json_t *)args, strings[i][0]));
        if (v && v[0]) json_set(out, strings[i][1], json_string(v));
    }
    return out;
}

json_t *w13_mainagent_07d4b1_monitoring_lines(const struct json_t *mon, int otlp_available) {
    json_t *m = w13_mainagent_07d4b1_dict_or_empty(mon);
    json_t *out = json_array();
    char line[1024], tmp[256], tmp2[256];
#define PUSH(s) json_append(out, json_string(s))
    PUSH("Gateway monitoring");
    json_t *gh = w13_mainagent_07d4b1_dict_or_empty(json_obj_get(m, "gateway_health_export"));
    int enabled = truthy(json_obj_get(gh, "enabled"));
    snprintf(line, sizeof(line), "  Health export:  %s (monitoring.gateway_health_export.enabled)",
             enabled ? "enabled" : "disabled");
    PUSH(line);
    if (enabled) {
        json_t *v = json_obj_get(gh, "metrics_enabled");
        snprintf(tmp, sizeof(tmp), "%s", v ? (truthy(v) ? "on" : "off") : "on");
        py_scalar(json_obj_get(gh, "export_interval_seconds") ? json_obj_get(gh, "export_interval_seconds") : json_number(60), tmp2, sizeof(tmp2));
        snprintf(line, sizeof(line), "    Metrics:            %s (interval %ss)", tmp, tmp2);
        PUSH(line);
        v = json_obj_get(gh, "diagnostic_events_enabled");
        snprintf(tmp, sizeof(tmp), "%s", v ? (truthy(v) ? "on" : "off") : "on");
        snprintf(line, sizeof(line), "    Diagnostic events:  %s", tmp);
        PUSH(line);
        v = json_obj_get(gh, "warning_error_events_enabled");
        snprintf(tmp, sizeof(tmp), "%s", v ? (truthy(v) ? "on" : "off") : "on");
        py_scalar(json_obj_get(gh, "logs_export_interval_seconds") ? json_obj_get(gh, "logs_export_interval_seconds") : json_number(5), tmp2, sizeof(tmp2));
        snprintf(line, sizeof(line), "    Warning/error logs: %s (interval %ss)", tmp, tmp2);
        PUSH(line);
        PUSH("    Content safety:     always on (rendered messages are never exported; not configurable)");
    }
    json_t *ex = w13_mainagent_07d4b1_dict_or_empty(json_obj_get(m, "export"));
    json_t *otlp = w13_mainagent_07d4b1_dict_or_empty(json_obj_get(ex, "otlp"));
    const char *endpoint = jstr(json_obj_get(otlp, "endpoint"));
    if (!endpoint) endpoint = "";
    if (truthy(json_obj_get(otlp, "enabled")) && endpoint[0]) {
        snprintf(line, sizeof(line), "  OTLP endpoint:  %s", endpoint);
    } else {
        snprintf(line, sizeof(line), "  OTLP endpoint:  not configured (monitoring.export.otlp)");
    }
    PUSH(line);
    snprintf(line, sizeof(line), "  OTel SDK:       %s (optional extra: hermes-agent[otlp])",
             otlp_available ? "installed" : "not installed");
    PUSH(line);
    PUSH("");
    PUSH("  Scope: gateway service health + redacted diagnostics only.");
    PUSH("  No prompts, messages, tool args/results, usage analytics, or traces.");
#undef PUSH
    json_free(gh);
    json_free(ex);
    json_free(otlp);
    json_free(m);
    return out;
}
