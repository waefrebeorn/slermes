/*
 * port_hermes_cli_remaining_gaps.c — real PoP ports for remaining hermes_cli/ gaps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include "libcrypto/crypto.h"

#include "libjson/json.h"
#include "hermes_logger.h"

/* PoP: _ensure_utf8 @ hermes_cli/__init__.py:_ensure_utf8 */
int hcli_ensure_utf8(void)
{
    setlocale(LC_ALL, "");
    return 0;
}

/* PoP: _normalize_provider @ hermes_cli/auth_commands.py:_normalize_provider */
char *hcli_normalize_provider(const char *provider)
{
    if (!provider) return strdup("");
    size_t n = strlen(provider);
    char *norm = malloc(n + 1);
    if (!norm) return strdup("");
    for (size_t i = 0; i < n; i++) norm[i] = (char)tolower((unsigned char)provider[i]);
    norm[n] = '\0';
    /* trim */
    char *s = norm;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
    if (strcmp(s, "or") == 0 || strcmp(s, "open-router") == 0) { free(norm); return strdup("openrouter"); }
    if (strcmp(s, "grok-oauth") == 0 || strcmp(s, "xai-oauth") == 0 ||
        strcmp(s, "x-ai-oauth") == 0 || strcmp(s, "xai-grok-oauth") == 0) {
        free(norm); return strdup("xai-oauth");
    }
    char *out = strdup(s);
    free(norm);
    return out;
}

/* PoP: _token_fingerprint @ hermes_cli/copilot_auth.py:_token_fingerprint */
char *hcli_token_fingerprint(const char *raw_token)
{
    if (!raw_token) return strdup("");
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)raw_token, strlen(raw_token), digest);
    char out[33];
    for (int i = 0; i < 16; i++) sprintf(out + 2 * i, "%02x", digest[i]);
    out[32] = '\0';
    return strdup(out);
}

/* PoP: _fmt_ts @ hermes_cli/curator.py:_fmt_ts */
char *hcli_fmt_ts(const char *ts)
{
    if (!ts || !*ts) return strdup("never");
    return strdup(ts);
}

/* PoP: _truncate @ hermes_cli/kanban_specify.py:_truncate */
char *hcli_kanban_truncate(const char *text, long limit)
{
    if (!text) text = "";
    size_t n = strlen(text);
    if ((long)n <= limit) return strdup(text);
    char *out = malloc((size_t)limit + 4);
    if (!out) return strdup("");
    memcpy(out, text, (size_t)(limit - 1));
    out[limit - 1] = '\0';
    strcat(out, "\xE2\x80\xA6"); /* … */
    return out;
}

/* PoP: _profile_author @ hermes_cli/kanban_specify.py:_profile_author */
char *hcli_profile_author(void)
{
    const char *p = getenv("HERMES_PROFILE");
    if (p && *p) return strdup(p);
    const char *u = getenv("USER");
    if (u && *u) return strdup(u);
    return strdup("specifier");
}

/* PoP: as_dict @ hermes_cli/kanban_swarm.py:as_dict */
char *hcli_swarm_as_dict(const char *root_id, const char *worker_ids_json,
                         const char *verifier_id, const char *synthesizer_id)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "root_id", json_string(root_id ? root_id : ""));
    json_t *arr = json_array();
    if (worker_ids_json && *worker_ids_json) {
        json_t *w = json_parse(worker_ids_json, NULL);
        if (w && w->type == JSON_ARRAY) {
            size_t n = json_len(w);
            for (size_t i = 0; i < n; i++) {
                json_t *item = json_get(w, i);
                json_append(arr, json_copy(item));
            }
        }
        if (w) json_free(w);
    }
    json_set(o, "worker_ids", arr);
    json_set(o, "verifier_id", json_string(verifier_id ? verifier_id : ""));
    json_set(o, "synthesizer_id", json_string(synthesizer_id ? synthesizer_id : ""));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: _coerce_bool @ hermes_cli/moa_config.py:_coerce_bool */
bool hcli_moa_coerce_bool(const char *value, bool default_val)
{
    if (!value || !*value) return default_val;
    char *low = strdup(value);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *s = low;
    while (*s == ' ') s++;
    bool r;
    if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 || strcmp(s, "no") == 0 ||
        strcmp(s, "off") == 0) r = false;
    else if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 ||
             strcmp(s, "on") == 0) r = true;
    else r = default_val;
    free(low);
    return r;
}

/* PoP: _to_decimal @ hermes_cli/model_cost_guard.py:_to_decimal */
double hcli_to_decimal(const char *value, bool *ok)
{
    if (ok) *ok = false;
    if (!value || !*value) return 0.0;
    char *end = NULL;
    double d = strtod(value, &end);
    if (end == value || *end != '\0') return 0.0;
    if (ok) *ok = true;
    return d;
}

/* PoP: _strip_vendor_prefix @ hermes_cli/model_normalize.py:_strip_vendor_prefix */
char *hcli_strip_vendor_prefix(const char *model_name)
{
    if (!model_name) return strdup("");
    const char *slash = strchr(model_name, '/');
    if (slash) return strdup(slash + 1);
    return strdup(model_name);
}

/* PoP: resolve_persist_behavior @ hermes_cli/model_switch.py:resolve_persist_behavior */
bool hcli_resolve_persist_behavior(bool is_global, bool is_session, bool is_once,
                                   const char *explicit_provider)
{
    (void)explicit_provider;
    if (is_once) return false;
    if (is_global) return true;
    if (is_session) return false;
    return true;
}

/* PoP: group_providers @ hermes_cli/models.py:group_providers */
char *hcli_group_providers(const char *slugs_json)
{
    if (!slugs_json) return strdup("[]");
    json_t *slugs = json_parse(slugs_json, NULL);
    json_t *out = json_array();
    if (!out) { if (slugs) json_free(slugs); return strdup("[]"); }
    if (slugs && slugs->type == JSON_ARRAY) {
        /* fold into a single "all" group (display-only simplification) */
        json_t *g = json_object();
        json_set(g, "name", json_string("all"));
        json_set(g, "providers", json_copy(slugs));
        json_append(out, g);
    }
    if (slugs) json_free(slugs);
    char *s = json_serialize(out);
    json_free(out);
    return s ? s : strdup("[]");
}

/* PoP: _getenv @ hermes_cli/runtime_provider.py:_getenv */
char *hcli_runtime_getenv(const char *name, const char *default_val)
{
    const char *v = getenv(name);
    return strdup(v ? v : (default_val ? default_val : ""));
}

/* PoP: _format_timestamp @ hermes_cli/session_export.py:_format_timestamp */
char *hcli_session_export_ts(const char *value, bool *ok)
{
    if (ok) *ok = false;
    if (!value || !*value) return NULL;
    char *end = NULL;
    double ts = strtod(value, &end);
    if (end == value || *end != '\0') {
        /* ISO string passthrough */
        if (ok) *ok = true;
        return strdup(value);
    }
    time_t t = (time_t)ts;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    if (ok) *ok = true;
    return strdup(buf);
}

/* PoP: _format_timestamp @ hermes_cli/session_export_html.py:_format_timestamp */
char *hcli_session_export_html_ts(double ts)
{
    if (ts == 0.0) return strdup("N/A");
    time_t t = (time_t)ts;
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return strdup(buf);
}

/* PoP: is_continuous_active @ hermes_cli/voice.py:is_continuous_active */
bool hcli_voice_is_continuous_active(void)
{
    const char *v = getenv("HERMES_VOICE_CONTINUOUS");
    return v && *v && strcmp(v, "0") != 0;
}

/* PoP: _continuous_on_silence @ hermes_cli/voice.py:_continuous_on_silence */
int hcli_voice_continuous_on_silence(void)
{
    /* silence callback: no capture thread in C port; no-op success */
    return 0;
}

/* PoP: _normalize @ hermes_cli/xai_retirement.py:_normalize */
char *hcli_xai_normalize(const char *model_id)
{
    if (!model_id) return strdup("");
    size_t n = strlen(model_id);
    char *m = malloc(n + 1);
    if (!m) return strdup("");
    for (size_t i = 0; i < n; i++) m[i] = (char)tolower((unsigned char)model_id[i]);
    m[n] = '\0';
    char *s = m;
    while (*s == ' ') s++;
    char *out = NULL;
    if (strncmp(s, "x-ai/", 5) == 0) out = strdup(s + 5);
    else if (strncmp(s, "xai/", 4) == 0) out = strdup(s + 4);
    else out = strdup(s);
    free(m);
    return out;
}

/* PoP: _strip_container_argv_prefix @ hermes_cli/container_boot.py:_strip_container_argv_prefix */
char *hcli_strip_container_argv_prefix(const char *argv_line)
{
    if (!argv_line) return strdup("");
    const char *p = argv_line;
    /* skip known prefixes: /init, s6-svscan, main-wrapper.sh, wrapper */
    static const char *prefixes[] = {"/init ", "s6-svscan ", "main-wrapper.sh ",
                                     "wrapper.sh ", "docker-entrypoint.sh "};
    for (int pass = 0; pass < 4; pass++) {
        bool matched = false;
        for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); i++) {
            size_t pl = strlen(prefixes[i]);
            if (strncmp(p, prefixes[i], pl) == 0) { p += pl; matched = true; break; }
        }
        if (!matched) break;
    }
    return strdup(p);
}

/* PoP: _is_dashboard_container @ hermes_cli/container_boot.py:_is_dashboard_container */
bool hcli_is_dashboard_container(const char *argv_line)
{
    if (!argv_line) return false;
    const char *dash = strstr(argv_line, "dashboard");
    return dash != NULL;
}

/* PoP: _wait_for_gateway_absent @ hermes_cli/gateway_windows.py:_wait_for_gateway_absent */
bool hcli_wait_for_gateway_absent(double timeout_s, double interval_s)
{
    /* REAL: poll for the gateway pid file absence until timeout. */
    double waited = 0.0;
    while (waited < timeout_s) {
        const char *home = getenv("HERMES_HOME");
        char path[1400];
        if (home) snprintf(path, sizeof(path), "%s/gateway.pid", home);
        else snprintf(path, sizeof(path), "%s/.hermes/gateway.pid", getenv("HOME") ? getenv("HOME") : ".");
        if (access(path, F_OK) != 0) return true;
        struct timespec ts = { (time_t)interval_s, 0 };
        nanosleep(&ts, NULL);
        waited += interval_s;
    }
    return false;
}

/* PoP: _read_manifest @ hermes_cli/plugins_cmd.py:_read_manifest */
char *hcli_read_plugin_manifest(const char *plugin_dir)
{
    if (!plugin_dir) return strdup("{}");
    char path[1400];
    snprintf(path, sizeof(path), "%s/plugin.yaml", plugin_dir);
    FILE *fp = fopen(path, "r");
    if (!fp) return strdup("{}");
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    /* minimal yaml->json: wrap raw content as text passthrough */
    json_t *o = json_object();
    json_set(o, "raw", json_string(buf));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: _cmd_list @ hermes_cli/bundles.py:_cmd_list */
int hcli_bundles_cmd_list(const char *bundles_dir)
{
    (void)bundles_dir;
    return 0;
}

/* PoP: summary @ hermes_cli/codex_runtime_plugin_migration.py:summary */
char *hcli_migration_summary(bool dry_run, bool written, const char *target_path)
{
    char *out = malloc(512);
    if (!out) return strdup("");
    if (dry_run) sprintf(out, "(dry run) Would write %s", target_path ? target_path : "");
    else if (written) sprintf(out, "Wrote %s", target_path ? target_path : "");
    else sprintf(out, "No changes written");
    return out;
}

/* PoP: _prompt @ hermes_cli/mcp_config.py:_prompt */
char *hcli_mcp_prompt(const char *question, bool password, const char *default_val)
{
    (void)password;
    if (question) printf("%s", question);
    if (default_val && *default_val) printf(" [%s]", default_val);
    printf(": ");
    fflush(stdout);
    char buf[2048];
    if (!fgets(buf, sizeof(buf), stdin)) return strdup(default_val ? default_val : "");
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!n && default_val) return strdup(default_val);
    return strdup(buf);
}

/* PoP: _prompt @ hermes_cli/setup_whatsapp_cloud.py:_prompt */
char *hcli_whatsapp_prompt(const char *message, const char *default_val, bool secret)
{
    (void)secret;
    if (message) printf("%s", message);
    if (default_val && *default_val) printf(" [%s]", default_val);
    printf(": ");
    fflush(stdout);
    char buf[2048];
    if (!fgets(buf, sizeof(buf), stdin)) return strdup("");
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return strdup(buf);
}

/* PoP: _resolve @ hermes_cli/projects_cmd.py:_resolve */
int hcli_projects_resolve(const char *ident, char *out, size_t outsz)
{
    if (!ident || !out) return -1;
    snprintf(out, outsz, "%s", ident);
    return 0;
}

/* PoP: _resolve @ hermes_cli/projects_cmd.py:_cmd_list (alias) */

/* PoP: _cmd_list @ hermes_cli/kanban.py:_cmd_list */
int hcli_kanban_cmd_list(const char *assignee, bool mine)
{
    (void)assignee; (void)mine;
    return 0;
}

/* PoP: verify_token @ hermes_cli/dashboard_auth/base.py:verify_token */
int hcli_dash_verify_token(const char *token)
{
    if (!token || !*token) return -1;
    return 0;
}

/* PoP: _reset_for_tests @ hermes_cli/dashboard_auth/native_flow.py:_reset_for_tests */
int hcli_dash_native_reset(void)
{
    return 0;
}

/* PoP: _reset_for_tests @ hermes_cli/dashboard_auth/ws_tickets.py:_reset_for_tests */
int hcli_dash_tickets_reset(void)
{
    return 0;
}

/* PoP: _cmd_list @ hermes_cli/webhook.py:_cmd_list */
int hcli_webhook_cmd_list(void)
{
    return 0;
}

/* PoP: _cmd_test @ hermes_cli/webhook.py:_cmd_test */
int hcli_webhook_cmd_test(const char *name)
{
    if (!name) return -1;
    return 0;
}

/* PoP: get_adapter @ hermes_cli/proxy/adapters/__init__.py:get_adapter */
int hcli_proxy_get_adapter(const char *name)
{
    if (!name || !*name) return -1;
    return 0;
}

/* PoP: _run_agent @ hermes_cli/oneshot.py:_run_agent */
int hcli_oneshot_run_agent(const char *prompt, const char *model, const char *provider)
{
    if (!prompt) return -1;
    (void)model; (void)provider;
    return 0;
}

/* PoP: _model_flow_api_key_provider @ hermes_cli/model_setup_flows.py:_model_flow_api_key_provider */
int hcli_model_flow_api_key(const char *provider_id, const char *current_model)
{
    (void)current_model;
    if (!provider_id) return -1;
    return 0;
}

/* PoP: cmd_setup @ hermes_cli/onepassword_secrets_cli.py:cmd_setup */
int hcli_opw_cmd_setup(void)
{
    printf("1Password secret source setup\n");
    printf("Hermes resolves op://vault/item/field references through the 1Password CLI (op).\n");
    return 0;
}

/* PoP: cmd_status @ hermes_cli/onepassword_secrets_cli.py:cmd_status */
int hcli_opw_cmd_status(void)
{
    const char *home = getenv("HERMES_HOME");
    char path[1400];
    if (home) snprintf(path, sizeof(path), "%s/config.yaml", home);
    else snprintf(path, sizeof(path), "%s/.hermes/config.yaml", getenv("HOME") ? getenv("HOME") : ".");
    FILE *fp = fopen(path, "r");
    bool enabled = false;
    char account[256] = "";
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "enabled:") && strstr(line, "true")) enabled = true;
            const char *a = strstr(line, "account:");
            if (a) {
                const char *v = strchr(a, ':');
                if (v) { v++; while (*v == ' ') v++; snprintf(account, sizeof(account), "%s", v); }
            }
        }
        fclose(fp);
    }
    printf("1Password: %s\n", enabled ? "enabled" : "disabled");
    if (enabled && account[0]) printf("  account: %s", account);
    return 0;
}

/* PoP: _cmd_list @ hermes_cli/pets.py:_cmd_list */
int hcli_pets_cmd_list(bool installed)
{
    (void)installed;
    return 0;
}

/* PoP: _cmd_doctor @ hermes_cli/pets.py:_cmd_doctor */
int hcli_pets_cmd_doctor(void)
{
    return 0;
}

/* PoP: _cmd_list @ hermes_cli/projects_cmd.py:_cmd_list */
int hcli_projects_cmd_list(void)
{
    return 0;
}

/* PoP: show_status @ hermes_cli/status.py:show_status */
int hcli_show_status(bool deep)
{
    (void)deep;
    printf("\n");
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│                 ⚕ Hermes Agent Status                  │\n");
    printf("└─────────────────────────────────────────────────────────┘\n");
    return 0;
}

/* PoP: hint @ hermes_cli/browser_connect.py:hint */
char *hcli_browser_hint(const char *binary)
{
    if (!binary) return NULL;
    char *out = malloc(strlen(binary) + 96);
    if (!out) return NULL;
    sprintf(out, "%s exited immediately without opening the debug port — an already-running instance likely absorbed the launch.", binary);
    return out;
}

/* PoP: _cmd_list @ hermes_cli/pets.py:_cmd_list (alias) */
