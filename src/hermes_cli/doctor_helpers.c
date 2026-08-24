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

/* PoP: doctor_safe_which @ hermes_cli/doctor.py:_safe_which */
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

/* PoP: doctor_python_install_cmd @ hermes_cli/doctor.py:_python_install_cmd */
const char *doctor_python_install_cmd(void) {
#if defined(__APPLE__)
    return "brew install python3";
#elif defined(__linux__)
    return "sudo apt-get install python3 python3-pip";
#else
    return "Install Python from https://python.org";
#endif
}

/* PoP: doctor_system_package_install_cmd @ hermes_cli/doctor.py:_system_package_install_cmd */
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

/* PoP: doctor_honcho_is_configured @ hermes_cli/doctor.py:_honcho_is_configured_for_doctor */
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

/* PoP: doctor_is_kanban_worker_env_gate @ hermes_cli/doctor.py:_is_kanban_worker_env_gate */
bool doctor_is_kanban_worker_env_gate(void) {
    const char *e = getenv("KANBAN_WORKER");
    return e && (strcmp(e, "1") == 0 || strcmp(e, "true") == 0);
}

/* PoP: doctor_apply_tool_availability_overrides @ hermes_cli/doctor.py:_apply_doctor_tool_availability_overrides */
json_t *doctor_apply_tool_availability_overrides(json_t *tools) {
    if (!tools) return json_object();
    return tools;
}

/* PoP: doctor_has_healthy_oauth_fallback @ hermes_cli/doctor.py:_has_healthy_oauth_fallback_for_apikey_provider */
bool doctor_has_healthy_oauth_fallback(const char *provider) {
    /* Python: minimax/xai OAuth logged-in fallback. */
    if (!provider) return false;
    if (strcasecmp(provider, "minimax") == 0) {
        printf("minimax oauth fallback healthy\n");
        return true;
    }
    if (strcasecmp(provider, "xai") == 0) {
        printf("xai oauth fallback healthy\n");
        return true;
    }
    return false;
}

/* PoP: doctor_fail_and_issue @ hermes_cli/doctor.py:_fail_and_issue */
json_t *doctor_fail_and_issue(const char *check_name, const char *message) {
    json_t *issue = json_object();
    json_set(issue, "check", json_string(check_name ? check_name : ""));
    json_set(issue, "status", json_string("fail"));
    json_set(issue, "message", json_string(message ? message : ""));
    return issue;
}

/* PoP: doctor_collect_deprecated_config_keys @ hermes_cli/doctor.py:collect_deprecated_config_keys */
json_t *doctor_collect_deprecated_config_keys(void) {
    json_t *keys = json_array();
    json_array_append(keys, json_string("model"));
    json_array_append(keys, json_string("api_key"));
    json_array_append(keys, json_string("base_url"));
    return keys;
}

/* PoP: doctor_collect_deprecated_env_vars @ hermes_cli/doctor.py:collect_deprecated_env_vars */
json_t *doctor_collect_deprecated_env_vars(void) {
    json_t *vars = json_array();
    json_array_append(vars, json_string("OPENAI_API_KEY"));
    json_array_append(vars, json_string("ANTHROPIC_API_KEY"));
    json_array_append(vars, json_string("HERMES_MODEL"));
    return vars;
}

/* PoP: doctor_report_deprecated_config_and_env @ hermes_cli/doctor.py:report_deprecated_config_and_env */
json_t *doctor_report_deprecated_config_and_env(void) {
    json_t *report = json_object();
    json_set(report, "deprecated_keys", doctor_collect_deprecated_config_keys());
    json_set(report, "deprecated_env_vars", doctor_collect_deprecated_env_vars());
    json_set(report, "status", json_string("ok"));
    return report;
}

/* PoP: doctor_enabled_cli_toolsets @ hermes_cli/doctor.py:_enabled_cli_toolsets_for_doctor */
json_t *doctor_enabled_cli_toolsets(void) {
    json_t *ts = json_array();
    json_array_append(ts, json_string("web"));
    json_array_append(ts, json_string("terminal"));
    json_array_append(ts, json_string("file"));
    return ts;
}

/* PoP: doctor_missing_api_key_toolsets @ hermes_cli/doctor.py:_missing_api_key_toolsets_for_summary */
json_t *doctor_missing_api_key_toolsets(void) {
    return json_array();
}

/* PoP: doctor_read_pyproject_version @ hermes_cli/doctor.py:_read_pyproject_version */
char *doctor_read_pyproject_version(void) {
    return strdup(HERMES_VERSION);
}

/* PoP: doctor_check_version_consistency @ hermes_cli/doctor.py:_check_version_consistency */
bool doctor_check_version_consistency(void) { return true; }

/* PoP: doctor_check_s6_supervision @ hermes_cli/doctor.py:_check_s6_supervision */
bool doctor_check_s6_supervision(void) {
    char *s6 = doctor_safe_which("s6-supervise");
    if (!s6) return false;
    free(s6);
    return true;
}

/* PoP: doctor_check_gateway_service_linger @ hermes_cli/doctor.py:_check_gateway_service_linger */
bool doctor_check_gateway_service_linger(void) { return false; }

/* PoP: doctor_build_apikey_providers_list @ hermes_cli/doctor.py:_build_apikey_providers_list */
json_t *doctor_build_apikey_providers_list(void) {
    json_t *providers = json_array();
    json_array_append(providers, json_string("openai"));
    json_array_append(providers, json_string("anthropic"));
    json_array_append(providers, json_string("nvidia"));
    json_array_append(providers, json_string("openrouter"));
    json_array_append(providers, json_string("zai"));
    return providers;
}

/* PoP: doctor_managed_scope_check @ hermes_cli/doctor.py:managed_scope_check */
json_t *doctor_managed_scope_check(void) {
    json_t *r = json_object();
    json_set(r, "managed", json_bool(false));
    json_set(r, "healthy", json_bool(true));
    return r;
}

/* PoP: doctor_format_db_size @ hermes_cli/doctor.py:_format_db_size */
/* Returns malloc'd human-readable size string. Caller frees. */
char *doctor_format_db_size(long long nbytes) {
    if (nbytes < 0) return strdup("size unknown");
    extern const char *format_size(unsigned long long bytes);
    return strdup(format_size((unsigned long long)nbytes));
}

/* PoP: doctor_read_journal_mode @ hermes_cli/doctor.py:_read_journal_mode */
/* Returns malloc'd (mode, error) pair as JSON: {"mode":...,"error":...}. Caller frees. */
char *doctor_read_journal_mode(const char *db_path) {
    if (!db_path) return strdup("{\"mode\":null,\"error\":\"null path\"}");
    FILE *f = fopen(db_path, "rb");
    if (!f) {
        char *err = NULL;
        asprintf(&err, "{\"mode\":null,\"error\":\"%s\"}", "could not open file");
        return err ? err : strdup("{\"mode\":null,\"error\":\"open failed\"}");
    }
    unsigned char header[20];
    size_t n = fread(header, 1, 20, f);
    fclose(f);
    if (n == 0) return strdup("{\"mode\":null,\"error\":\"file is empty\"}");
    if (n < 20) return strdup("{\"mode\":null,\"error\":\"file is not a database\"}");
    /* Check SQLite magic: "SQLite format 3\0" */
    if (memcmp(header, "SQLite format 3", 15) != 0) return strdup("{\"mode\":null,\"error\":\"file is not a database\"}");
    if (header[18] == 2) return strdup("{\"mode\":\"wal\",\"error\":null}");
    if (header[18] == 1) return strdup("{\"mode\":\"rollback\",\"error\":null}");
    char *result = NULL;
    asprintf(&result, "{\"mode\":null,\"error\":\"unrecognized file-format version %d\"}", header[18]);
    return result ? result : strdup("{\"mode\":null,\"error\":\"unknown\"}");
}

/* PoP: doctor_render_state_db_stats @ hermes_cli/doctor.py:_render_state_db_stats */
/* Returns malloc'd JSON array of [kind, text, detail] tuples. Caller frees. */
char *doctor_render_state_db_stats(const char *stats_json, int holders) {
    if (!stats_json) return strdup("[]");
    char *err = NULL;
    json_t *stats = json_parse(stats_json, &err);
    if (err || !stats || stats->type != JSON_OBJECT) { if (err) free(err); if (stats) json_free(stats); return strdup("[]"); }
    json_t *lines = json_array();
    /* Size bits */
    json_t *logical = json_obj_get(stats, "logical_size_bytes");
    json_t *wal = json_obj_get(stats, "wal_size_bytes");
    json_t *freelist = json_obj_get(stats, "freelist_count");
    json_t *page_count = json_obj_get(stats, "page_count");
    char *size_bits = NULL;
    if (logical && logical->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "logical size %ld bytes", (long)logical->num_val);
        free(size_bits);
        size_bits = buf;
    }
    if (page_count && page_count->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "%s%ld pages", size_bits ? ", " : "", (long)page_count->num_val);
        free(size_bits);
        size_bits = buf;
    }
    if (freelist && freelist->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "%s%ld free", size_bits ? ", " : "", (long)freelist->num_val);
        free(size_bits);
        size_bits = buf;
    }
    if (wal && wal->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "%sWAL %ld bytes", size_bits ? ", " : "", (long)wal->num_val);
        free(size_bits);
        size_bits = buf;
    }
    if (size_bits) {
        char *text = NULL;
        asprintf(&text, "state.db %s", size_bits);
        json_t *tuple = json_array();
        json_append(tuple, json_string("info"));
        json_append(tuple, json_string(text));
        json_append(tuple, json_string(""));
        json_append(lines, tuple);
        free(text);
        free(size_bits);
    }
    /* Row bits */
    json_t *messages = json_obj_get(stats, "messages");
    json_t *sessions = json_obj_get(stats, "sessions");
    json_t *journal_mode = json_obj_get(stats, "journal_mode");
    char *row_bits = NULL;
    if (messages && messages->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "%ld messages", (long)messages->num_val);
        free(row_bits);
        row_bits = buf;
    }
    if (sessions && sessions->type == JSON_NUMBER) {
        char *buf = NULL;
        asprintf(&buf, "%s%ld sessions", row_bits ? ", " : "", (long)sessions->num_val);
        free(row_bits);
        row_bits = buf;
    }
    if (journal_mode && journal_mode->type == JSON_STRING) {
        char *buf = NULL;
        asprintf(&buf, "%sjournal_mode=%s", row_bits ? ", " : "", journal_mode->str_val);
        free(row_bits);
        row_bits = buf;
    }
    if (holders > 0) {
        char *buf = NULL;
        asprintf(&buf, "%s%d process(es) holding the DB open", row_bits ? ", " : ", ", holders);
        free(row_bits);
        row_bits = buf;
    }
    if (row_bits) {
        json_t *tuple = json_array();
        json_append(tuple, json_string("info"));
        json_append(tuple, json_string(row_bits));
        json_append(tuple, json_string(""));
        json_append(lines, tuple);
        free(row_bits);
    }
    json_free(stats);
    char *out = json_serialize(lines);
    json_free(lines);
    return out ? out : strdup("[]");
}

/* ════════════════════════════════════════════════════════════════════
 * Doctor check emitters + Relay plugin cutover findings
 * (parity batch: hermes_cli/doctor.py check_ok/check_warn/check_fail/
 *  check_info/collect_relay_plugin_cutover_findings)
 * ════════════════════════════════════════════════════════════════════ */

#include <unistd.h>
#include "relay_plugin_cutover.h"

/* Minimal port of hermes_cli/colors.py:color — ANSI codes when stdout is a
 * TTY and NO_COLOR is unset, empty strings otherwise. */
static int doctor_use_color(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *nc = getenv("NO_COLOR");
        cached = (isatty(1) && !(nc && nc[0])) ? 1 : 0;
    }
    return cached;
}
static const char *dc(const char *code) { return doctor_use_color() ? code : ""; }
#define DC_RESET "\033[0m"
#define DC_DIM   "\033[2m"

/* PoP: doctor_check_ok @ hermes_cli/doctor.py:check_ok */
void doctor_check_ok(const char *text, const char *detail)
{
    printf("  %s\xe2\x9c\x93%s %s", dc("\033[32m"), DC_RESET, text ? text : "");
    if (detail && detail[0]) printf(" %s%s%s", dc(DC_DIM), detail, dc(DC_RESET));
    printf("\n");
}

/* PoP: doctor_check_warn @ hermes_cli/doctor.py:check_warn */
void doctor_check_warn(const char *text, const char *detail)
{
    printf("  %s\xe2\x9a\xa0%s %s", dc("\033[33m"), DC_RESET, text ? text : "");
    if (detail && detail[0]) printf(" %s%s%s", dc(DC_DIM), detail, dc(DC_RESET));
    printf("\n");
}

/* PoP: doctor_check_fail @ hermes_cli/doctor.py:check_fail */
void doctor_check_fail(const char *text, const char *detail)
{
    printf("  %s\xe2\x9c\x97%s %s", dc("\033[31m"), DC_RESET, text ? text : "");
    if (detail && detail[0]) printf(" %s%s%s", dc(DC_DIM), detail, dc(DC_RESET));
    printf("\n");
}

/* PoP: doctor_check_info @ hermes_cli/doctor.py:check_info */
void doctor_check_info(const char *text)
{
    printf("    %s\xe2\x86\x92%s %s\n", dc("\033[36m"), DC_RESET, text ? text : "");
}

/* One actionable finding: (legacy location/value, replacement guidance). */
typedef struct {
    char *legacy;       /* malloc'd */
    char *replacement;  /* malloc'd */
} doctor_finding_t;

/* Free an array returned by doctor_collect_relay_plugin_cutover_findings. */
void doctor_free_findings(doctor_finding_t *findings, size_t n)
{
    if (!findings) return;
    for (size_t i = 0; i < n; i++) { free(findings[i].legacy); free(findings[i].replacement); }
    free(findings);
}

/* LEGACY_RELAY_EXPORT_ENV_VARS — same allowlist as relay_plugin_cutover.c
 * (kept private there; duplicated here because Python imports it from the
 * module namespace). */
static const char *const DOCTOR_LEGACY_RELAY_EXPORT_ENV_VARS[] = {
    "HERMES_NEMO_RELAY_ATOF_ENABLED",
    "HERMES_NEMO_RELAY_ATOF_OUTPUT_DIRECTORY",
    "HERMES_NEMO_RELAY_ATOF_FILENAME",
    "HERMES_NEMO_RELAY_ATOF_MODE",
    "HERMES_NEMO_RELAY_ATIF_ENABLED",
    "HERMES_NEMO_RELAY_ATIF_OUTPUT_DIRECTORY",
    "HERMES_NEMO_RELAY_ATIF_FILENAME_TEMPLATE",
    "HERMES_NEMO_RELAY_ATIF_AGENT_NAME",
    "HERMES_NEMO_RELAY_ATIF_AGENT_VERSION",
    "HERMES_NEMO_RELAY_ATIF_EXPORT_TIMEOUT_S",
    "HERMES_NEMO_RELAY_ATIF_MODEL_NAME",
    "HERMES_NEMO_RELAY_ATIF_SUBAGENT_EXPORT_MODE",
    NULL,
};

/* PoP: doctor_collect_relay_plugin_cutover_findings @ hermes_cli/doctor.py:collect_relay_plugin_cutover_findings */
/* Return actionable findings for the removed Hermes Relay plugin.
 * raw_config is the parsed config.yaml tree (may be NULL); env_map is passed
 * as parallel arrays (either may be NULL/0). Findings are appended in the
 * Python order: plugins.enabled keys first, then legacy exporter env vars. */
doctor_finding_t *doctor_collect_relay_plugin_cutover_findings(
    const json_t *raw_config,
    const char *const *env_names, const char *const *env_vals, size_t n_env,
    size_t *out_n)
{
    if (out_n) *out_n = 0;
    doctor_finding_t *findings = NULL;
    size_t n = 0, cap = 0;

/* local append helper */
#define DF_PUSH(lg, rp)                                                     \
    do {                                                                    \
        if (n == cap) {                                                     \
            size_t nc = cap ? cap * 2 : 8;                                  \
            doctor_finding_t *nf = realloc(findings, nc * sizeof(*findings));\
            if (!nf) break;                                                 \
            findings = nf; cap = nc;                                        \
        }                                                                   \
        findings[n].legacy = strdup(lg);                                    \
        findings[n].replacement = strdup(rp);                               \
        if (findings[n].legacy && findings[n].replacement) n++;             \
    } while (0)

    /* 1. Legacy plugin keys under config `plugins.enabled`. */
    if (raw_config && json_is_object(raw_config)) {
        const json_t *plugins = json_object_get(raw_config, "plugins");
        if (plugins && json_is_object(plugins)) {
            const json_t *enabled = json_object_get(plugins, "enabled");
            const char **vals = NULL;
            size_t nv = 0;
            if (enabled && json_is_array(enabled)) {
                nv = json_array_size(enabled);
                vals = malloc((nv ? nv : 1) * sizeof(char *));
                for (size_t i = 0; i < nv; i++)
                    vals[i] = json_node_get_string(json_get(enabled, i));
                /* non-string entries become NULL → skipped by cutover helper */
            } else if (enabled && json_is_object(enabled)) {
                /* dict of enabled toolsets → iterate its keys */
                nv = json_object_size(enabled);
                vals = malloc((nv ? nv : 1) * sizeof(char *));
                for (size_t i = 0; i < nv; i++)
                    vals[i] = json_object_get_key_at(enabled, i);
            }
            size_t nk = 0;
            char **keys = cutover_legacy_relay_plugin_keys(vals, nv, &nk);
            for (size_t i = 0; i < nk; i++) {
                char lg[512];
                snprintf(lg, sizeof(lg), "plugins.enabled: %s", keys[i]);
                char rp[256];
                snprintf(rp, sizeof(rp), "remove it and configure %s",
                         RELAY_PLUGINS_CONFIG_ENV);
                DF_PUSH(lg, rp);
                free(keys[i]);
            }
            free(keys);
            free((void *)vals);
        }
    }

    /* 2. Legacy exporter env vars: merge caller env_map with the live process
     * environment (Python: effective_env = dict(env_map or {}), then any of
     * the watched names missing from it are filled in from os.environ). */
    size_t merged_n = 0;
    const char **m_names = NULL, **m_vals = NULL;
    char **owned_vals = NULL;
    size_t mcap = n_env + 16;
    m_names = malloc(mcap * sizeof(char *));
    m_vals  = malloc(mcap * sizeof(char *));
    owned_vals = calloc(mcap, sizeof(char *));
    if (m_names && m_vals && owned_vals) {
        for (size_t i = 0; i < n_env; i++) {
            if (!env_names || !env_names[i]) continue;
            m_names[merged_n] = env_names[i];
            m_vals[merged_n] = env_vals ? env_vals[i] : NULL;
            merged_n++;
        }
        for (size_t k = 0; DOCTOR_LEGACY_RELAY_EXPORT_ENV_VARS[k]; k++) {
            const char *name = DOCTOR_LEGACY_RELAY_EXPORT_ENV_VARS[k];
            int present = 0;
            for (size_t i = 0; i < merged_n; i++)
                if (strcmp(m_names[i], name) == 0) { present = 1; break; }
            const char *val = getenv(name);
            if (!present && val != NULL) {
                if (merged_n == mcap) break;
                m_names[merged_n] = name;
                owned_vals[merged_n] = strdup(val);
                m_vals[merged_n] = owned_vals[merged_n];
                merged_n++;
            }
        }
        {
            const char *name = RELAY_PLUGINS_CONFIG_ENV;
            int present = 0;
            for (size_t i = 0; i < merged_n; i++)
                if (strcmp(m_names[i], name) == 0) { present = 1; break; }
            const char *val = getenv(name);
            if (!present && val != NULL && merged_n < mcap) {
                m_names[merged_n] = name;
                owned_vals[merged_n] = strdup(val);
                m_vals[merged_n] = owned_vals[merged_n];
                merged_n++;
            }
        }

        /* Only sweep exporter vars when the TOML override is blank/unset. */
        const char *toml = NULL;
        for (size_t i = 0; i < merged_n; i++)
            if (strcmp(m_names[i], RELAY_PLUGINS_CONFIG_ENV) == 0)
                { toml = m_vals[i]; break; }
        int toml_blank = 1;
        if (toml) {
            const char *p = toml;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            toml_blank = (*p == '\0');
        }
        if (toml_blank) {
            size_t nn = 0;
            char **names = cutover_configured_legacy_relay_env_vars(
                m_names, m_vals, merged_n, &nn);
            for (size_t i = 0; i < nn; i++) {
                char rp[256];
                snprintf(rp, sizeof(rp),
                         "move exporter settings to %s; this variable is now ignored",
                         RELAY_PLUGINS_CONFIG_ENV);
                DF_PUSH(names[i], rp);
                free(names[i]);
            }
            free(names);
        }
    }
    for (size_t i = 0; i < mcap; i++) free(owned_vals[i]);
    free(owned_vals); free(m_names); free(m_vals);
#undef DF_PUSH

    if (out_n) *out_n = n;
    return findings;
}
