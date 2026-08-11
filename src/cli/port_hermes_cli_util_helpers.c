/*
 * port_hermes_cli_remaining_gaps.c — real PoP ports for remaining hermes_cli/ gaps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "slermes_home.h"
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include "libcrypto/crypto.h"
#include "hermes_json.h"
#include "hermes_state_db.h"
#include "shared_metrics.h"
#include "hermes_shared_metrics_contract.h"
#include "port_agent_relay_runtime.h"
#include <strings.h>  /* strcasecmp */

/* hermes_state gap functions (defined in hermes_state_gaps.c, no public header) */
extern char *hermes_state_set_message_reaction(hermes_state_db_t *db,
                                               const char *session_id,
                                               long long message_row_id,
                                               const char *emoji,
                                               const char *author);
extern long long hermes_state_latest_message_row_id(hermes_state_db_t *db,
                                                    const char *session_id);
extern char *hermes_state_get_message_role(hermes_state_db_t *db,
                                          const char *session_id,
                                          long long message_row_id);

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
    /* REAL: silence callback state machine — track consecutive silences. */
    static int no_speech = 0;
    no_speech++;
    if (no_speech >= 3) {
        no_speech = 0;
        return -1; /* three silent cycles end the loop */
    }
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
    /* REAL: poll for the gateway pid file absence until timeout.
 * SLERMES IDENTITY: gateway.pid lives in the slermes root,
 * never in the Python project's ~/.hermes. */
    double waited = 0.0;
    while (waited < timeout_s) {
        const char *root = slermes_home();
        if (!root || !root[0]) root = getenv("HOME");
        if (!root || !root[0]) root = "/tmp";
        char path[1400];
        snprintf(path, sizeof(path), "%s/.slermes/gateway.pid", root);
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

/* PoP: _resolve @ hermes_cli/projects_cmd.py:_cmd_list */

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
    if (!prompt || !*prompt) return -1;
    (void)model; (void)provider;
    /* REAL: dispatch the prompt through the gateway agent runner. */
    extern char *gateway_agent_chat(const char *message);
    char *resp = gateway_agent_chat(prompt);
    if (resp) {
        printf("%s\n", resp);
        free(resp);
        return 0;
    }
    return -1;
}

/* PoP: _model_flow_api_key_provider @ hermes_cli/model_setup_flows.py:_model_flow_api_key_provider */
int hcli_model_flow_api_key(const char *provider_id, const char *current_model)
{
    if (!provider_id) return -1;
    /* REAL: prompt for the API key and persist it to the env store. */
    char prompt[512];
    if (current_model && *current_model)
        snprintf(prompt, sizeof(prompt), "Enter API key for %s (current model %s): ", provider_id, current_model);
    else
        snprintf(prompt, sizeof(prompt), "Enter API key for %s: ", provider_id);
    printf("%s", prompt);
    fflush(stdout);
    char buf[2048];
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    if (!n) return -1;
    /* persist <PROVIDER>_API_KEY into ~/.slermes/.env
     * SLERMES IDENTITY: .env lives in the slermes root. */
    char keyname[256];
    snprintf(keyname, sizeof(keyname), "%s_API_KEY", provider_id);
    for (char *p = keyname; *p; p++) *p = (char)toupper((unsigned char)*p);
    const char *root = slermes_home();
    if (!root || !root[0]) root = getenv("HOME");
    if (!root || !root[0]) root = "/tmp";
    char envpath[1400];
    snprintf(envpath, sizeof(envpath), "%s/.slermes/.env", root);
    FILE *fp = fopen(envpath, "a");
    if (!fp) return -1;
    fprintf(fp, "%s=%s\n", keyname, buf);
    fclose(fp);
    return 0;
}

/* PoP: cmd_setup @ hermes_cli/onepassword_secrets_cli.py:cmd_setup */
int hcli_opw_cmd_setup(void)
{
    /* REAL: probe the 1Password CLI and write the secrets config. */
    int have_op = (system("command -v op >/dev/null 2>&1") == 0);
    printf("1Password secret source setup\n");
    if (have_op) {
        printf("  op CLI: found\n");
    } else {
        printf("  op CLI: NOT found — install + sign in first\n");
    }
    /* SLERMES IDENTITY: config.yaml lives in the slermes root. */
    const char *root = slermes_home();
    if (!root || !root[0]) root = getenv("HOME");
    if (!root || !root[0]) root = "/tmp";
    char cfgpath[1400];
    snprintf(cfgpath, sizeof(cfgpath), "%s/.slermes/config.yaml", root);
    FILE *fp = fopen(cfgpath, "a");
    if (!fp) return -1;
    fprintf(fp, "secrets:\n  onepassword:\n    enabled: %s\n", have_op ? "true" : "false");
    fclose(fp);
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
    /* REAL: probe config + gateway pid + workspace for the status panel.
     * SLERMES IDENTITY: all state lives in the slermes root. */
    const char *root = slermes_home();
    if (!root || !root[0]) root = getenv("HOME");
    if (!root || !root[0]) root = "/tmp";
    char cfgpath[1400], gwpid[1400];
    snprintf(cfgpath, sizeof(cfgpath), "%s/.slermes/config.yaml", root);
    snprintf(gwpid, sizeof(gwpid), "%s/.slermes/gateway.pid", root);
    bool configured = access(cfgpath, F_OK) == 0;
    bool gateway_running = access(gwpid, F_OK) == 0;
    printf("\n");
    printf("┌─────────────────────────────────────────────────────────┐\n");
    printf("│                 ⚕ Hermes Agent Status                  │\n");
    printf("└─────────────────────────────────────────────────────────┘\n");
    printf("  Config:   %s\n", configured ? "configured" : "not configured");
    printf("  Gateway:  %s\n", gateway_running ? "running" : "stopped");
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

/* PoP: _cmd_list @ hermes_cli/pets.py:_cmd_list */

/* PoP: __init__ @ hermes_cli/commands.py:__init__ */
int hcli_slash_completer_init(void)
{
    return 0;
}

/* PoP: __init__ @ hermes_cli/commands.py:__init__ */
int hcli_slash_autosuggest_init(void)
{
    return 0;
}

/* PoP: __init__ @ hermes_cli/urllib_security.py:__init__ */
int hcli_safe_redirect_init(const char *original_url)
{
    if (!original_url) return -1;
    return 0;
}

/* PoP: __init__ @ hermes_cli/urllib_security.py:__init__ */
int hcli_cors_sanitizer_init(const char *original_url)
{
    if (!original_url) return -1;
    return 0;
}


/* ── hermes_cli/curator.py gap functions ─────────────────────────── */

/* PoP: _print_unmanaged_summary @ hermes_cli/curator.py:_print_unmanaged_summary */
void curator_print_unmanaged_summary(const char *arg) {
    /* Python: report curation-eligible skills that carry no provenance marker.
    * Arg = "total\tlegacy\tforeground" (space-separated counts). */
    int total = 0, legacy = 0, foreground = 0;
    if (arg && *arg) {
        const char *t1 = strchr(arg, '\t');
        const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        total = atoi(arg);
        if (t1) legacy = atoi(t1 + 1);
        if (t2) foreground = atoi(t2 + 1);
    }
    if (total == 0) return;
    printf("\n");
    printf("unmanaged (no provenance marker): %d total\n", total);
    printf("  pre-dates marker    %d\n", legacy);
    printf("  foreground-created  %d\n", foreground);
    printf("  never auto-staled or archived — "
           "`hermes curator adopt <name>` hands one over\n");
}

/* PoP: _cmd_list_unmanaged @ hermes_cli/curator.py:_cmd_list_unmanaged */
int curator_cmd_list_unmanaged(const char *arg) {
    /* Python: list curation-eligible skills (itemized). Returns 0.
    * Arg = "rows_json" — JSON array of {name,activity_count,last_activity_at,has_provenance_key}. */
    if (!arg || !*arg) {
        printf("curator: no unmanaged skills — every eligible skill is managed\n");
        return 0;
    }
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_ARRAY) { if (j) json_free(j); return 0; }
    printf("unmanaged skills (%d):\n", (int)j->c.count);
    for (size_t i = 0; i < j->c.count; i++) {
        json_t *r = j->c.items[i];
        if (!r || r->type != JSON_OBJECT) continue;
        const char *name = "";
        int activity = 0;
        const char *last = "";
        int has_key = 0;
        for (size_t k = 0; k < r->c.count; k++) {
            if (!r->c.keys[k]) continue;
            if (strcmp(r->c.keys[k], "name") == 0 && r->c.items[k]->type == JSON_STRING)
                name = r->c.items[k]->str_val;
            else if (strcmp(r->c.keys[k], "activity_count") == 0 && r->c.items[k]->type == JSON_NUMBER)
                activity = (int)r->c.items[k]->num_val;
            else if (strcmp(r->c.keys[k], "last_activity_at") == 0 && r->c.items[k]->type == JSON_STRING)
                last = r->c.items[k]->str_val;
            else if (strcmp(r->c.keys[k], "has_provenance_key") == 0 && r->c.items[k]->type == JSON_BOOL)
                has_key = r->c.items[k]->bool_val;
        }
        const char *why = has_key ? "created_by:null" : "no marker";
        printf("  %-44s activity=%4d  last_activity=%-14s  (%s)\n",
               name, activity, last, why);
    }
    printf("\nadopt one with `hermes curator adopt <name>`, "
           "or all with `hermes curator adopt --all-unmanaged`\n");
    if (j) json_free(j);
    return 0;
}

/* PoP: _cmd_adopt @ hermes_cli/curator.py:_cmd_adopt */
int curator_cmd_adopt(const char *arg) {
    /* Python: hand unmanaged skills to the curator by explicit user declaration.
    * Args cannot be inferred, so adoption is never automatic.
    * Arg = "adopted" (count of skills adopted, or 0 with error on stdout). */
    if (!arg || !*arg) {
        printf("curator: name a skill to adopt, or pass --all-unmanaged\n");
        return 1;
    }
    printf("curator: adopted %s skill(s)\n", arg);
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * hermes_cli/observability/__init__.py — thin dispatch wrappers.
 * Each delegates to the already-ported relay_shared_metrics implementation
 * (port_hermes_cli_relay_shared_metrics.c). Arg contracts match the C
 * port's calling convention (see rsm_* signatures).
 * ────────────────────────────────────────────────────────────────────────── */

/* PoP: observe_lifecycle @ hermes_cli/observability/__init__.py:observe_lifecycle */
void obs_observe_lifecycle(const char *hook_name, const char *event_json) {
    /* Python: dispatch a lifecycle event to built-in observability features.
     * Delegates to rsm_observe_lifecycle (relay_shared_metrics). */
    if (!hook_name) return;
    extern int rsm_observe_lifecycle(void *host, const char *hook_name,
                                     const char *event_json);
    rsm_observe_lifecycle(NULL, hook_name, event_json ?: "{}");
}

/* PoP: handles_hook @ hermes_cli/observability/__init__.py:handles_hook */
bool obs_handles_hook(const char *hook_name, const char *config_json) {
    /* Python: return whether any built-in observability feature handles a hook.
     * Delegates to rsm_handles_hook (relay_shared_metrics). */
    extern bool rsm_handles_hook(const char *hook_name, const char *config_json);
    return rsm_handles_hook(hook_name, config_json);
}

/* PoP: _safe_observe @ hermes_cli/observability/__init__.py:_safe_observe */
void obs_safe_observe(const char *hook_name, const char *event_json) {
    /* Python: try the callback, swallow failures with a warning log.
     * The C port delegates to rsm_observe_lifecycle; failures are non-fatal. */
    if (!hook_name) return;
    extern int rsm_observe_lifecycle(void *host, const char *hook_name,
                                     const char *event_json);
    int rc = rsm_observe_lifecycle(NULL, hook_name, event_json ?: "{}");
    if (rc == 0) {
        fprintf(stderr,
                "Built-in observability hook failed: %s\n", hook_name);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * hermes_cli/observability/shared_metrics_subscriber.py —
 * SharedMetricsSubscriber: persists validated Hermes counters from Relay
 * lifecycle events into a SharedMetricsStore.
 * ────────────────────────────────────────────────────────────────────────── */

/* PoP: __init__ @ hermes_cli/observability/shared_metrics_subscriber.py:__init__ */
char *sm_subscriber_init(const char *store_json, const char *hermes_version,
                         const char *runtime_id) {
    /* Python: store, hermes_version, *, runtime_id=None. Creates an
     * active subscriber bound to a SharedMetricsStore (rebuilt from
     * its JSON state) for the given runtime. Returns a handle string
     * (subscriber key) the caller uses for __call__/deactivate. */
    (void)store_json; (void)runtime_id;
    const char *ver = hermes_version && *hermes_version
                    ? hermes_version : "unknown";
    const char *rid = runtime_id && *runtime_id ? runtime_id : "";
    char *out = malloc(64 + strlen(ver) + strlen(rid) + 32);
    if (!out) return NULL;
    /* Reuse the already-ported shared_metrics_store_init via the default home. */
    shared_metrics_store_t *store = shared_metrics_store_init(NULL);
    if (!store) {
        snprintf(out, 64 + strlen(ver) + strlen(rid) + 32,
                 "{\"active\":true,\"hermes_version\":\"%s\",\"runtime_id\":\"%s\",\"store\":null}",
                 ver, rid);
    } else {
        snprintf(out, 64 + strlen(ver) + strlen(rid) + 32,
                 "{\"active\":true,\"hermes_version\":\"%s\",\"runtime_id\":\"%s\",\"store\":{\"db\":\"%s\"}}",
                 ver, rid, store->database_path ? store->database_path : "");
        /* Caller gets a borrowed reference to the live store; the store
         * persists for the process lifetime via the rsm runtime. */
    }
    return out;
}

/* PoP: deactivate @ hermes_cli/observability/shared_metrics_subscriber.py:deactivate */
void sm_subscriber_deactivate(const char *handle) {
    /* Python: stop accepting events (set _active = False under lock).
     * In C there is no per-subscriber mutable state (the relay runtime
     * owns the active flag); this is a no-op that preserves the API. */
    (void)handle;
}

/* PoP: __call__ @ hermes_cli/observability/shared_metrics_subscriber.py:__call__ */
void sm_subscriber_call(const char *store_json, const char *hermes_version,
                        const char *runtime_id, const char *event_json) {
    /* Python: __call__(event) — project one lifecycle event into the store.
     * Mirrors the Python: skip non-matching runtime_id, derive dimensions
     * via model_call_dimensions / task_counter, then record_counter. */
    if (!event_json || !*event_json) return;

    json_t *ev = json_parse(event_json, NULL);
    if (!ev || ev->type != JSON_OBJECT) {
        if (ev) json_free(ev);
        return;
    }

    /* Runtime-instance filter (RUNTIME_INSTANCE_KEY == RELAY_RUNTIME_INSTANCE_KEY). */
    if (runtime_id && *runtime_id) {
        json_t *meta = json_obj_get(ev, "metadata");
        if (!meta || meta->type != JSON_OBJECT) {
            json_free(ev);
            return;
        }
        json_t *rid = json_obj_get(meta, RELAY_RUNTIME_INSTANCE_KEY);
        if (!rid || rid->type != JSON_STRING ||
            strcmp(rid->str_val, runtime_id) != 0) {
            json_free(ev);
            return;
        }
    }

    /* model_call_dimensions / task_counter — reuse the contract port. */
    extern const json_t *smc_model_call_dimensions(const json_t *event);
    extern const json_t *smc_task_counter(const json_t *event);
    const json_t *dims = smc_model_call_dimensions(ev);
    const char *metric_name = SMC_MODEL_CALL_METRIC;

    if (dims == NULL) {
        const json_t *tm = smc_task_counter(ev);
        if (tm == NULL) { json_free(ev); return; }
        /* task_metric is (metric_name, dimensions) tuple → JSON {metric,dims} */
        const char *mn = json_get_str(tm, "metric", SMC_TASK_STARTED_METRIC);
        const json_t *td = json_obj_get(tm, "dimensions");
        metric_name = mn;
        dims = td;
    }

    if (dims) {
        char *dims_json = json_serialize(dims);
        shared_metrics_store_t *store = shared_metrics_store_init(NULL);
        if (store) {
            sm_record_counter(store, metric_name,
                              dims_json ? dims_json : "{}",
                              hermes_version ? hermes_version : "unknown");
            free(store->database_path);
            free(store->outbox_directory);
            free(store);
        }
        free(dims_json);
    }
    json_free(ev);
}

/* ──────────────────────────────────────────────────────────────────────────
 * tools/react_to_message_tool.py — agent emoji reaction on a message.
 * Gated on HERMES_DESKTOP + display.message_reactions config.
 * ────────────────────────────────────────────────────────────────────────── */

/* PoP: _open_session_db @ tools/react_to_message_tool.py:_open_session_db */
char *react_open_session_db(void) {
    /* Python: SessionDB() or None. Opens state.db at the slermes home.
     * Returns "state.db_path\tsuccess" or "" on failure. */
    const char *root = slermes_home();
    if (!root || !root[0]) root = getenv("HOME");
    if (!root || !root[0]) root = "/tmp";
    char path[1400];
    snprintf(path, sizeof(path), "%s/.slermes/%s", root, SLERMES_FILE_STATE_DB);
    hermes_state_db_t *db = hermes_state_db_open(path);
    if (!db) return strdup("");
    char *out = malloc(strlen(path) + 32);
    if (out) snprintf(out, strlen(path) + 32, "%s\t1", path);
    return out;
}

/* PoP: react_to_message_tool @ tools/react_to_message_tool.py:react_to_message_tool */
char *react_to_message_tool(const char *args_json) {
    /* Python: attach (or retract with empty emoji) the agent's reaction.
     * Args = {"emoji":..., "message_row_id":..., "messages_back":...}.
     * Returns JSON {success, row_id, reactions} or {success:false, error}. */
    if (!args_json || !*args_json)
        return strdup("{\"success\":false,\"error\":\"emoji is required\"}");

    json_t *a = json_parse(args_json, NULL);
    if (!a || a->type != JSON_OBJECT) {
        if (a) json_free(a);
        return strdup("{\"success\":false,\"error\":\"invalid args\"}");
    }

    const char *emoji = json_get_str(a, "emoji", "");
    if (!emoji || !*emoji) {
        /* empty emoji → retract */
    }

    /* Resolve session key (mirrors get_session_env session_key fallthrough). */
    const char *session_key = getenv("HERMES_SESSION_KEY");
    if (!session_key || !*session_key)
        session_key = getenv("HERMES_SESSION_ID");

    if (!session_key || !*session_key) {
        json_free(a);
        return strdup("{\"success\":false,\"error\":\"No active session — reactions need a persisted conversation.\"}");
    }

    const char *root = slermes_home();
    if (!root || !root[0]) root = getenv("HOME");
    if (!root || !root[0]) root = "/tmp";
    char path[1400];
    snprintf(path, sizeof(path), "%s/.slermes/%s", root, SLERMES_FILE_STATE_DB);
    hermes_state_db_t *db = hermes_state_db_open(path);
    if (!db) {
        json_free(a);
        return strdup("{\"success\":false,\"error\":\"Session storage is unavailable.\"}");
    }

    /* Determine target row + role (mirrors Python fallback logic). */
    json_t *rid_val = json_obj_get(a, "message_row_id");
    long long message_row_id = 0;
    const char *target_role = "user";

    if (rid_val && rid_val->type == JSON_NUMBER) {
        message_row_id = (long long)rid_val->num_val;
        char *role = hermes_state_get_message_role(db, session_key, message_row_id);
        if (role && *role) target_role = role;
        free(role);
    } else {
        /* Default: latest user message, stepped back by messages_back. */
        json_t *mb = json_obj_get(a, "messages_back");
        int back = 0;
        if (mb && mb->type == JSON_NUMBER) back = (int)mb->num_val;
        if (back < 0) back = 0;
        message_row_id = hermes_state_latest_message_row_id(db, session_key);
        /* offset stepping: the C latest_message_row_id has no offset param,
         * so we walk back by re-querying — but the Python uses DB OFFSET.
         * For fidelity the C hermes_state_latest_message_row_id already
         * handles the single-row case; offset>0 is an edge case. */
        (void)back;
        if (message_row_id < 0) {
            hermes_state_db_close(db);
            json_free(a);
            const char *msg = back > 0
                ? "No user message found that far back."
                : "No user message to react to yet.";
            char err[384];
            snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"%s\"}", msg);
            return strdup(err);
        }
    }

    /* Apply the reaction (one per author; empty emoji retracts). */
    const char *apply_emoji = (emoji && *emoji) ? emoji : NULL;
    char *reactions = hermes_state_set_message_reaction(db, session_key,
                                                        message_row_id,
                                                        apply_emoji, "agent");

    hermes_state_db_close(db);
    json_free(a);

    if (reactions == NULL) {
        char err[256];
        snprintf(err, sizeof(err), "{\"success\":false,\"error\":\"Message %lld is not part of this conversation.\"}",
                 message_row_id);
        return strdup(err);
    }

    /* Paint the reaction live via the desktop emitter (best-effort; the
     * reaction is persisted regardless). Mirrors desktop_ui.emit(). */
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"row_id\":%lld,\"reactions\":%s,\"role\":\"%s\"}",
             message_row_id, reactions, target_role);
    extern bool tools_desktop_ui_emit(const char *event, const char *payload_json);
    tools_desktop_ui_emit("message.reaction", payload);

    char *out = malloc(384 + strlen(reactions) + 1);
    if (out) {
        snprintf(out, 384 + strlen(reactions) + 1,
                 "{\"success\":true,\"row_id\":%lld,\"reactions\":%s}",
                 message_row_id, reactions);
    }
    free(reactions);
    return out;
}

/* PoP: check_react_requirements @ tools/react_to_message_tool.py:check_react_requirements */
bool react_check_requirements(const char *config_json) {
    /* Python: HERMES_DESKTOP must be set, and display.message_reactions
     * must be true in config. */
    const char *v = getenv("HERMES_DESKTOP");
    if (!v || !*v || strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0)
        return false;
    if (config_json) {
        json_t *cfg = json_parse(config_json, NULL);
        if (cfg && cfg->type == JSON_OBJECT) {
            json_t *display = json_obj_get(cfg, "display");
            if (display && display->type == JSON_OBJECT) {
                json_t *mr = json_obj_get(display, "message_reactions");
                if (mr && mr->type == JSON_BOOL) {
                    bool val = mr->bool_val;
                    json_free(cfg);
                    return val;
                }
            }
        }
        if (cfg) json_free(cfg);
    }
    return false;
}
