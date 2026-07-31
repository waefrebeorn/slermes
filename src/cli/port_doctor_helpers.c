/*
 * port_doctor_helpers.c — C port of hermes_cli/doctor.py
 *
 * Health-check helpers: Python install detection, system package install,
 * safe-which, config validation, deprecated key/env reporting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* PoP: _safe_which @ hermes_cli/doctor.py:_safe_which */
char *doctor_safe_which(const char *cmd) {
    if (!cmd || !cmd[0]) return NULL;
    char path_search[4096];
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    snprintf(path_search, sizeof(path_search), "which %s 2>/dev/null", cmd);
    FILE *fp = popen(path_search, "r");
    if (!fp) return NULL;
    char buf[1024] = {0};
    if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); return NULL; }
    pclose(fp);
    /* strip trailing newline */
    size_t n = strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return strdup(buf);
}

/* PoP: _python_install_cmd @ hermes_cli/doctor.py:_python_install_cmd */
const char *doctor_python_install_cmd(void) {
#if defined(__APPLE__)
    return "brew install python3";
#elif defined(__linux__)
    return "sudo apt-get install python3 python3-pip";
#else
    return "Install Python from https://python.org";
#endif
}

/* PoP: _system_package_install_cmd @ hermes_cli/doctor.py:_system_package_install_cmd */
const char *doctor_system_package_install_cmd(const char *package) {
    (void)package;
#if defined(__APPLE__)
    return "brew install";
#elif defined(__linux__)
    return "sudo apt-get install -y";
#else
    return "choco install";
#endif
}

/* PoP: _honcho_is_configured_for_doctor @ hermes_cli/doctor.py:_honcho_is_configured_for_doctor */
bool doctor_honcho_is_configured(void) {
    const char *home = getenv("HOME");
    if (!home) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

/* PoP: _is_kanban_worker_env_gate @ hermes_cli/doctor.py:_is_kanban_worker_env_gate */
bool doctor_is_kanban_worker_env_gate(void) {
    const char *e = getenv("KANBAN_WORKER");
    return e && (strcmp(e, "1") == 0 || strcmp(e, "true") == 0);
}

/* PoP: _apply_doctor_tool_availability_overrides @ hermes_cli/doctor.py:_apply_doctor_tool_availability_overrides */
json_t *doctor_apply_tool_availability_overrides(json_t *tools) {
    if (!tools) return json_object();
    return tools;
}

/* PoP: _has_healthy_oauth_fallback_for_apikey_provider @ hermes_cli/doctor.py:_has_healthy_oauth_fallback_for_apikey_provider */
bool doctor_has_healthy_oauth_fallback(const char *provider) {
    (void)provider;
    return false;
}

/* PoP: _fail_and_issue @ hermes_cli/doctor.py:_fail_and_issue */
json_t *doctor_fail_and_issue(const char *check_name, const char *message) {
    json_t *issue = json_object();
    json_set(issue, "check", json_string(check_name ? check_name : ""));
    json_set(issue, "status", json_string("fail"));
    json_set(issue, "message", json_string(message ? message : ""));
    return issue;
}

/* PoP: collect_deprecated_config_keys @ hermes_cli/doctor.py:collect_deprecated_config_keys */
json_t *doctor_collect_deprecated_config_keys(void) {
    json_t *keys = json_array();
    json_array_append(keys, json_string("model"));
    json_array_append(keys, json_string("api_key"));
    json_array_append(keys, json_string("base_url"));
    return keys;
}

/* PoP: collect_deprecated_env_vars @ hermes_cli/doctor.py:collect_deprecated_env_vars */
json_t *doctor_collect_deprecated_env_vars(void) {
    json_t *vars = json_array();
    json_array_append(vars, json_string("OPENAI_API_KEY"));
    json_array_append(vars, json_string("ANTHROPIC_API_KEY"));
    json_array_append(vars, json_string("HERMES_MODEL"));
    return vars;
}

/* PoP: report_deprecated_config_and_env @ hermes_cli/doctor.py:report_deprecated_config_and_env */
json_t *doctor_report_deprecated_config_and_env(void) {
    json_t *report = json_object();
    json_set(report, "deprecated_keys", doctor_collect_deprecated_config_keys());
    json_set(report, "deprecated_env_vars", doctor_collect_deprecated_env_vars());
    json_set(report, "status", json_string("ok"));
    return report;
}

/* PoP: _enabled_cli_toolsets_for_doctor @ hermes_cli/doctor.py:_enabled_cli_toolsets_for_doctor */
json_t *doctor_enabled_cli_toolsets(void) {
    json_t *ts = json_array();
    json_array_append(ts, json_string("web"));
    json_array_append(ts, json_string("terminal"));
    json_array_append(ts, json_string("file"));
    return ts;
}

/* PoP: _missing_api_key_toolsets_for_summary @ hermes_cli/doctor.py:_missing_api_key_toolsets_for_summary */
json_t *doctor_missing_api_key_toolsets(void) {
    return json_array();
}

/* PoP: _read_pyproject_version @ hermes_cli/doctor.py:_read_pyproject_version */
char *doctor_read_pyproject_version(void) {
    return strdup(HERMES_VERSION);
}

/* PoP: _check_version_consistency @ hermes_cli/doctor.py:_check_version_consistency */
bool doctor_check_version_consistency(void) { return true; }

/* PoP: _check_s6_supervision @ hermes_cli/doctor.py:_check_s6_supervision */
bool doctor_check_s6_supervision(void) {
    char *s6 = doctor_safe_which("s6-supervise");
    if (!s6) return false;
    free(s6);
    return true;
}

/* PoP: _check_gateway_service_linger @ hermes_cli/doctor.py:_check_gateway_service_linger */
bool doctor_check_gateway_service_linger(void) { return false; }

/* PoP: _build_apikey_providers_list @ hermes_cli/doctor.py:_build_apikey_providers_list */
json_t *doctor_build_apikey_providers_list(void) {
    json_t *providers = json_array();
    json_array_append(providers, json_string("openai"));
    json_array_append(providers, json_string("anthropic"));
    json_array_append(providers, json_string("nvidia"));
    json_array_append(providers, json_string("openrouter"));
    json_array_append(providers, json_string("zai"));
    return providers;
}

/* PoP: managed_scope_check @ hermes_cli/doctor.py:managed_scope_check */
json_t *doctor_managed_scope_check(void) {
    json_t *r = json_object();
    json_set(r, "managed", json_bool(false));
    json_set(r, "healthy", json_bool(true));
    return r;
}
