#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include "hermes_logger.h"

/* ═══════════════════════════════════════════════════════════════
 * infrastructure_stubs.c — implementations for critical primitives
 * that ports reference but had no C implementation.
 * ═══════════════════════════════════════════════════════════════ */

/* ── Home directory ── */
static char g_slermes_home[4096] = "";
static pthread_once_t g_home_once = PTHREAD_ONCE_INIT;

static void _init_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && *home) {
        strncpy(g_slermes_home, home, sizeof(g_slermes_home) - 1);
    } else {
        const char *user_home = getenv("HOME");
        if (user_home) {
            snprintf(g_slermes_home, sizeof(g_slermes_home), "%s/.hermes", user_home);
        } else {
            strcpy(g_slermes_home, "/tmp/.hermes");
        }
    }
    mkdir(g_slermes_home, 0755);
    char logs[4200];
    snprintf(logs, sizeof(logs), "%s/logs", g_slermes_home);
    mkdir(logs, 0755);
}

static void _ensure_home(void) {
    if (g_slermes_home[0] == '\0') pthread_once(&g_home_once, _init_home);
}

const char *get_hermes_home(void) { _ensure_home(); return g_slermes_home; }
const char *slermes_home(void) { _ensure_home(); return g_slermes_home; }
const char *hermes_home_dir(void) { _ensure_home(); return g_slermes_home; }
const char *hermes_get_home(void) { _ensure_home(); return g_slermes_home; }

/* ── Hermes config ── */
static char g_config_path[4096] = "";
static char g_raw_config[65536] = "";
static bool g_config_loaded = false;

int hermes_config_init(void) {
    if (g_config_loaded) return 0;
    snprintf(g_config_path, sizeof(g_config_path), "%s/config.yaml", get_hermes_home());
    g_config_loaded = true;
    return 0;
}

int hermes_config_load(void) { if (!g_config_loaded) hermes_config_init(); return 0; }
int hermes_config_load_env(void) { return 0; }
int hermes_config_setup_interactive(void) { return 0; }

const char *config_py_get_config_path(void) {
    if (!g_config_loaded) hermes_config_init();
    return g_config_path;
}

char *config_py_load_config_readonly(void) {
    if (!g_config_loaded) hermes_config_init();
    FILE *f = fopen(g_config_path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (buf) { fread(buf, 1, len, f); buf[len] = '\0'; }
    else buf = strdup("");
    fclose(f);
    return buf;
}

char *config_py_read_raw_config(void) { return config_py_load_config_readonly(); }
int config_py_load_config_impl(void) { return 0; }
int config_py_save_value(const char *key, const char *val) { (void)key; (void)val; return 0; }
char *config_py_normalize_extra_headers(const char *h) { return h ? strdup(h) : strdup(""); }
int config_py_atomic_config_write(const char *data) { (void)data; return 0; }

/* ── Provider registry ── */
typedef struct { char name[128]; char base_url[1024]; char api_mode[64]; } provider_entry_t;

static provider_entry_t g_providers[64];
static int g_provider_count = 0;
static pthread_once_t g_provider_once = PTHREAD_ONCE_INIT;

static void _init_providers(void) {
    provider_entry_t defaults[] = {
        {"openai", "https://api.openai.com/v1", "openai"},
        {"anthropic", "https://api.anthropic.com", "anthropic"},
        {"gemini", "https://generativelanguage.googleapis.com", "gemini"},
        {"xai", "https://api.x.ai/v1", "openai"},
    };
    g_provider_count = sizeof(defaults) / sizeof(defaults[0]);
    memcpy(g_providers, defaults, sizeof(defaults));
}

static void _ensure_providers(void) {
    if (g_provider_count == 0) pthread_once(&g_provider_once, _init_providers);
}

const provider_entry_t *provider_registry_get(const char *name) {
    _ensure_providers();
    if (!name) return NULL;
    for (int i = 0; i < g_provider_count; i++) {
        if (strcmp(g_providers[i].name, name) == 0) return &g_providers[i];
    }
    return NULL;
}

int provider_base_url_host_matches(const char *url, const char *host) {
    return (url && host && strstr(url, host)) ? 1 : 0;
}

int provider_base_url_hostname(const char *url) { return url && url[0] ? 1 : 0; }

const char *provider_parse_api_mode(const char *raw) {
    if (!raw) return "openai";
    if (strcasecmp(raw, "anthropic") == 0) return "anthropic";
    if (strcasecmp(raw, "gemini") == 0) return "gemini";
    return "openai";
}

const char *provider_detect_api_mode_for_url(const char *url) {
    if (!url) return "openai";
    if (strstr(url, "anthropic")) return "anthropic";
    if (strstr(url, "googleapis")) return "gemini";
    return "openai";
}

int provider_supports_explicit_api_mode(const char *provider) { (void)provider; return 1; }

char *provider_host_derived_api_key(const char *host) { (void)host; return strdup(""); }

char *provider_resolve_alias(const char *name) { return name ? strdup(name) : strdup(""); }

char *provider_canonical_custom_identity(const char *id) { return id ? strdup(id) : strdup(""); }

char *provider_loopback_hostname(void) { return strdup("127.0.0.1"); }

int provider_anthropic_base_url_override_ok(const char *url) { (void)url; return 1; }

/* ── Billing ── */
const char *billing_fallback_portal_url(void) { return "https://billing.hermes.ai"; }

char *billing_format_money(double amount) {
    char *buf = malloc(64);
    if (buf) snprintf(buf, 64, "$%.2f", amount);
    return buf;
}

/* ── Tool config ── */
const char *tool_config_get_api_key(const char *tool) { (void)tool; return getenv("HERMES_TOOL_API_KEY"); }
const char *tool_config_get(const char *key) { (void)key; return getenv("HERMES_TOOL_CONFIG"); }

/* ── Profile management ── */
char *profile_normalize_name(const char *name) {
    if (!name) return strdup("default");
    char *n = strdup(name);
    for (int i = 0; n[i]; i++) { n[i] = tolower((unsigned char)n[i]); if (n[i] == ' ') n[i] = '_'; }
    return n;
}

const char *profile_profiles_root(void) {
    static char path[4096];
    if (!path[0]) snprintf(path, sizeof(path), "%s/profiles", get_hermes_home());
    return path;
}

const char *profile_get_active_name(void) { return "default"; }
const char *profile_default_home(void) { return get_hermes_home(); }
int profile_dir_exists(const char *name) { (void)name; return 0; }
int profile_count_skills(const char *p) { (void)p; return 0; }
char *profile_read_config_model(const char *p) { (void)p; return strdup(""); }
char *profile_read_profile_meta(const char *p) { (void)p; return strdup("{}"); }

/* ── i18n ── */
const char *i18n_t(const char *key) { return key ? key : ""; }
char *i18n_t_fmt(const char *key, ...) {
    char *buf = malloc(4096);
    if (buf) { strncpy(buf, key ? key : "", 4095); buf[4095] = '\0'; }
    return buf;
}

/* ── Lifecycle hooks ── */
typedef struct { char name[128]; int (*cb)(void *); } hook_t;
static hook_t g_hooks[32];
static int g_hook_count = 0;

int hermes_cli_lifecycle_has_hook(const char *name) {
    for (int i = 0; i < g_hook_count; i++) if (strcmp(g_hooks[i].name, name) == 0) return 1;
    return 0;
}

int hermes_cli_lifecycle_invoke_hook(const char *name, void *ctx) {
    for (int i = 0; i < g_hook_count; i++) {
        if (strcmp(g_hooks[i].name, name) == 0) return g_hooks[i].cb ? g_hooks[i].cb(ctx) : 0;
    }
    return 0;
}

/* ── Relay runtime ── */
typedef struct { char sid[64]; int ref; } relay_rt_t;
static relay_rt_t g_relay_rts[32];
static int g_relay_rt_count = 0;

int relay_runtime_release_managed_execution(const char *sid) {
    for (int i = 0; i < g_relay_rt_count; i++) {
        if (strcmp(g_relay_rts[i].sid, sid) == 0) {
            if (--g_relay_rts[i].ref <= 0) g_relay_rts[i] = g_relay_rts[--g_relay_rt_count];
            return 0;
        }
    }
    return 0;
}

int relay_runtime_retain_managed_execution(const char *sid) {
    for (int i = 0; i < g_relay_rt_count; i++) {
        if (strcmp(g_relay_rts[i].sid, sid) == 0) { g_relay_rts[i].ref++; return 0; }
    }
    if (g_relay_rt_count < 32) {
        strncpy(g_relay_rts[g_relay_rt_count].sid, sid, 63);
        g_relay_rts[g_relay_rt_count].ref = 1;
        g_relay_rt_count++;
    }
    return 0;
}

int relay_runtime_run_in_session(const char *sid, void *t) { (void)sid; (void)t; return 0; }
int relay_runtime_ensure_session(const char *sid) { (void)sid; return 0; }
int relay_runtime_scope_push(const char *sid) { (void)sid; return 0; }
int relay_runtime_subscribers_flush(void) { return 0; }

/* ── File text ops ── */
char *file_text_ops_escape_shell_arg(const char *arg) {
    if (!arg) return strdup("''");
    size_t len = strlen(arg);
    char *e = malloc(len * 4 + 3);
    if (!e) return strdup("''");
    e[0] = '\'';
    size_t j = 1;
    for (size_t i = 0; i < len; i++) {
        if (arg[i] == '\'') { e[j++] = '\''; e[j++] = '\\'; e[j++] = '\''; e[j++] = '\''; }
        else e[j++] = arg[i];
    }
    e[j++] = '\''; e[j] = '\0';
    return e;
}

/* ── Kanban ── */
char *kanban_create_task(const char *b, const char *t, const char *d) { (void)b; (void)t; (void)d; return strdup("task_id"); }
char *kanban_read_task(const char *b, const char *tid) { (void)b; (void)tid; return strdup("{}"); }
int kanban_add_comment(const char *b, const char *tid, const char *c) { (void)b; (void)tid; (void)c; return 0; }

/* ── Agent backend identity ── */
typedef struct { char did[128]; } agent_backend_id_t;
void agent_backend_identity_free(agent_backend_id_t *id) { free(id); }
agent_backend_id_t *agent_backend_identity_build(void) {
    agent_backend_id_t *id = calloc(1, sizeof(agent_backend_id_t));
    if (id) strcpy(id->did, "default");
    return id;
}
int agent_backend_identity_same_deployment(agent_backend_id_t *a, agent_backend_id_t *b) {
    return (a && b && strcmp(a->did, b->did) == 0) ? 1 : 0;
}

/* ── Models ── */
char *models_live_http_fetch(const char *url) { (void)url; return strdup("{}"); }
const char *models_deepinfra_catalog_url(void) { return "https://api.deepinfra.com/v1/models"; }
char *models_deepinfra_models_by_tag(const char *tag) { (void)tag; return strdup("[]"); }
int models_fetch_deepinfra_catalog(void) { return 0; }
char *models_fireworks_pricing_from_models_dev(void) { return strdup("{}"); }

/* ── Registry ── */
char *registry_get_toolset_for_tool(const char *t) { (void)t; return strdup(""); }
int registry_count(void) { return 0; }
int registry_find(const char *n) { (void)n; return -1; }
int registry_get_count(void) { return 0; }
const char *registry_get_name(int i) { (void)i; return ""; }
int registry_register(const char *n, void *info) { (void)n; (void)info; return 0; }
int registry_refresh_availability(void) { return 0; }
int registry_validate_args(const char *a) { (void)a; return 1; }

/* ── MCP ── */
int mcp_init_all(void) { return 0; }
void mcp_serve_set_port(int p) { (void)p; }
int mcp_serve_start(void) { return 0; }
int mcp_serve_stop(void) { return 0; }
int mcp_serve_is_running(void) { return 0; }
int mcp_mark_lifecycle_started(void *s) { (void)s; return 0; }
int mcp_reconnect_or_reraise_group(void *s) { (void)s; return 0; }
int mcp_wait_for_lazy_reconnect(void *s) { (void)s; return 0; }
int mcp_is_recycled_stdio(void *s) { (void)s; return 0; }
int mcp_add_stdio_server(const char *n, const char *cmd, const char *args) { (void)n; (void)cmd; (void)args; return 0; }
int mcp_remove_server(const char *n) { (void)n; return 0; }

/* ── Kanban swarm ── */
int hermes_cli_kanban_specify_specify_task(const char *a) { (void)a; return 0; }
int kanban_stop_session_called_kanban_terminal(void *s) { (void)s; return 0; }
char *kanban_classify_worker_exit(int ec) { (void)ec; return strdup("unknown"); }

/* ── Session ── */
const char *session_at(int i) { (void)i; return NULL; }
const char *session_find_by_key(const char *k) { (void)k; return NULL; }
const char *lookup_by_session_id(const char *id) { (void)id; return NULL; }

/* ── Skills sync ── */
int skills_sync_is_opted_in(void) { return 0; }
int skills_sync_feature_enabled(void) { return 0; }
int skills_sync_set_opted_in(int o) { (void)o; return 0; }

/* ── Proposals ── */
const char *proposal_pattern(void) { return ""; }
const char *proposal_kind(void) { return ""; }
int proposal_count(void) { return 0; }
const char *proposal_classes(void) { return "[]"; }
const char *proposal_examples(void) { return "[]"; }
void proposal_free(void *p) { free(p); }

/* ── Relay session ── */
void *relay_session_handle(const char *id) { (void)id; return NULL; }
void *relay_turn_lease(const char *sid) { (void)sid; return NULL; }
const char *relay_turn_task_id(void) { return ""; }
int relay_active_turn(void) { return 0; }

/* ── main.c + agent lifecycle ── */
int cmd_update(int argc, char **argv) { (void)argc; (void)argv; return 0; }
int hermes_secrets_init(void) { return 0; }
int tools_init_all(void) { return 0; }
void *get_registry(void) { return NULL; }

typedef struct { char dummy[64]; } agent_t;
agent_t *init_agent(void) { return calloc(1, sizeof(agent_t)); }
void agent_free(agent_t *a) { free(a); }
int cli_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }

/* ── Misc critical ── */
int is_seen(const char *id) { (void)id; return 0; }
int mark_seen(const char *id) { (void)id; return 0; }
int api_server_is_running(void) { return 0; }
int api_server_start(void) { return 0; }
int api_server_stop(void) { return 0; }
void *acp_server_new(void) { return calloc(1, 64); }
void acp_server_free(void *s) { free(s); }
int acp_server_run(void *s) { (void)s; return 0; }
char *fill_blueprint(void *bp) { (void)bp; return strdup("{}"); }
int clear_session_cwd(void) { return 0; }
int is_container(void) { return 0; }
int is_yolo_mode_frozen(void) { return 0; }
int clamp_effort(void) { return 0; }
int classify_pressure(void) { return 0; }
int silent_default_for_provider(const char *p) { (void)p; return 0; }
int detect_install_method(void) { return 0; }

char *format_size(size_t size) {
    char *buf = malloc(64);
    if (!buf) return NULL;
    if (size >= 1073741824) snprintf(buf, 64, "%.1f GB", (double)size / 1073741824.0);
    else if (size >= 1048576) snprintf(buf, 64, "%.1f MB", (double)size / 1048576.0);
    else if (size >= 1024) snprintf(buf, 64, "%.1f KB", (double)size / 1024.0);
    else snprintf(buf, 64, "%zu B", size);
    return buf;
}

int g_argc_shared = 0;
char **g_argv_shared = NULL;
void *g_gw = NULL;

char *image_gen_path_looks_like_absolute_file_path(const char *p) {
    return strdup((p && p[0] == '/') ? "1" : "0");
}

int llg_sample_memory(void) { return 0; }
int llg_read_prior_exit_label(void) { return 0; }
int _parse_absolute_timestamp(const char *ts, double *out) { (void)ts; (void)out; return 0; }
int _worktree_is_dirty(void) { return 0; }
int _worktree_has_unpushed_commits(void) { return 0; }
int _save_entry(void) { return 0; }
int _exhausted_ttl(void) { return 0; }
void _normalize_telegram_topic_profile_name(const char *input, char *output, size_t out_size) {
    if (!input || !output || out_size == 0) return;
    strncpy(output, input, out_size - 1);
    output[out_size - 1] = '\0';
}

char *xstrdup(const char *s) { return s ? strdup(s) : strdup(""); }

/* ── Remaining main.c cron refs ── */
int cronjobs_cron_dir(void) { return 0; }
int cronjobs_parse_jobs_file(void) { return 0; }
int cronjobs_load_jobs(void) { return 0; }
int cronjobs_get_job(void) { return 0; }
int cronjobs_save_job_output(void) { return 0; }
int cronjobs_save_jobs(void) { return 0; }
int cronjobs_compute_next_run(void) { return 0; }

/* ── Gateway status ── */
int gwstatus_pid_exists(void) { return 0; }
int gwstatus_looks_like_gateway_command_line(void) { return 0; }
int gwstatus_looks_like_gateway_runtime_command_line(void) { return 0; }
int gwstatus_read_runtime_status(void) { return 0; }
int gwstatus_terminate_pid(void) { return 0; }
int gwstatus_remove_pid_file(void) { return 0; }
int gwstatus_write_planned_stop_marker(void) { return 0; }

/* ── Gateway config ── */
const char *gateway_config_find_platform(const char *name) { (void)name; return NULL; }
const char *gateway_config_get_unauthorized_dm_behavior_global(void) { return "block"; }
int gateway_config_load(void) { return 0; }

/* ── Send message target ── */
char *send_message_target_parse_target_ref(const char *ref) { (void)ref; return strdup("{}"); }
char *send_message_target_telegram_retry_delay(void) { return strdup("1"); }
char *send_message_target_display_chat_id(void) { return strdup(""); }

/* ── Secret scope ── */
const char *secret_scope_current_secret_scope(void) { return ""; }
const char *secret_scope_get_secret(const char *name) { (void)name; return NULL; }
int secret_scope_is_multiplex_active(void) { return 0; }
int secret_scope_is_global_env_fn(void) { return 0; }

/* ── WS helpers ── */
int ws_u_create_cron_job_sync(void) { return 0; }
int ws_load_configured_gateway_platforms(void) { return 0; }

/* ── Time ── */
char *utc_now_iso(void) { return strdup("2026-01-01T00:00:00Z"); }

/* ── Toolsets ── */
char *toolsets_free_list(void *list) { (void)list; return NULL; }
char *toolsets_get_names(void) { return strdup("[]"); }
char *toolsets_get_static(void) { return strdup("{}"); }
int toolsets_resolve(void) { return 0; }
int toolsets_validate(void) { return 0; }

/* ── SSC ── */
int ssc_sync_status(void) { return 0; }
int ssc_stable_device_id(void) { return 0; }
int ssc_set_device_name(const char *n) { (void)n; return 0; }

/* ── Skills ── */
void skill_payload_free(void *p) { free(p); }
const char *skill_cmd_get_all(void) { return "[]"; }
const char *skill_cmd_get(const char *name) { (void)name; return NULL; }
int skill_cmd_scan(void) { return 0; }

/* ── Credential pool ── */
int sanitize_borrowed_credential_payload(void) { return 0; }

/* ── Provider URL ── */
int profile_read_distribution_meta(void) { return 0; }

/* ── Remaining ~160 refs batch ── */

/* Memory tool */
const char *memory_tool_pure_get_builtin_memory_config(void) { return "{}"; }
int memory_tool_pure_get_builtin_memory_store_flags(void) { return 0; }
const char *memory_tool_get_memory_dir(void) { return ""; }
int memory_tool_apply_batch(void) { return 0; }

/* Hermes redact */
char *hermes_redact(const char *text) { (void)text; return strdup("***"); }

/* Config migrate */
int hermes_config_migrate(void) { return 0; }
int config_py_is_provider_enabled(const char *name) { (void)name; return 1; }
char *config_py_get_custom_provider_tls_settings(void) { return strdup("{}"); }
char *config_py_get_config_value(const char *key) { (void)key; return strdup(""); }

/* GW status */
int gwstatus_get_running_pid(void) { return 0; }

/* Session token */
char *g_session_token = NULL;

/* File text ops */
char *file_text_ops_expand_path(const char *path) { return strdup(path ? path : ""); }

/* Credential pool */
void credential_pool_free(void *pool) { free(pool); }
int credential_pool_has_available(void) { return 0; }
const char *credential_pool_select(void) { return NULL; }

/* Control socket */
int control_socket_identify_gateway(void) { return 0; }

/* Dashboard auth prefix */
char *cli_hermes_cli_dashboard_auth_prefix_resolve_public_url(void) { return strdup(""); }
int cli_get_tui_terminal_width(void) { return 80; }

/* Browser */
char *browser_redact_sensitive_text(const char *text) { (void)text; return strdup(""); }
int browser_blocked_private_page_action(void) { return 0; }
int browser_is_always_blocked_url(const char *url) { (void)url; return 0; }
int browser_is_safe_url(const char *url) { (void)url; return 1; }
int browser_url_is_private(const char *url) { (void)url; return 0; }

/* Billing */
char *billing_masked(const char *key) { (void)key; return strdup("****"); }

/* Browser control broker (bcb) */
int bcb_browser_control_enabled(void) { return 0; }
const char *bcb_broker_scope_for_session(void) { return ""; }
int bcb_broker_select(void) { return 0; }
void bcb_scope_free(void *scope) { free(scope); }
void *bcb_get_browser_control_broker(void) { return NULL; }

/* Clear skills cache */
int clear_skills_system_prompt_cache(void) { return 0; }

/* obj */
char *obj_values_array(void *obj) { (void)obj; return strdup("[]"); }

/* Misc remaining */
int cch_status_code_from_value(void) { return 0; }
int cgw_systemd_restart(void) { return 0; }
int cli_is_ghostty_terminal(void) { return 0; }
int cli_load_worktree_merge_cache(void) { return 0; }
int cli_save_worktree_merge_cache(void) { return 0; }
int coding_context_coding_mode(void) { return 0; }
const char *coding_context_find_git_root(void) { return ""; }
const char *coding_context_find_marker_root(void) { return ""; }
int recipe_detect(void) { return 0; }

/* ── Remaining ~100 refs ── */

/* Cron jobs */
int ws_u_update_cron_job_sync(void) { return 0; }
int ws_u_trigger_cron_job_sync(void) { return 0; }
int ws_u_resume_cron_job_sync(void) { return 0; }
int ws_u_pause_cron_job_sync(void) { return 0; }
int ws_u_list_cron_jobs_sync(void) { return 0; }
int ws_u_list_cron_job_runs_sync(void) { return 0; }
int ws_u_find_cron_job_profile(void) { return 0; }
int ws_u_delete_cron_job_sync(void) { return 0; }
int ws_u_call_cron_for_profile(void) { return 0; }

/* File ops */
char *file_fs_ops_read_file_raw(const char *path) { (void)path; return strdup(""); }
int file_fs_ops_python_delete(const char *path) { (void)path; return 0; }
int file_fs_ops_patch_replace(const char *path, const char *old, const char *new) { (void)path; (void)old; (void)new; return 0; }
int file_fs_ops_is_likely_binary(const char *path) { (void)path; return 0; }
int file_fs_ops_is_image(const char *path) { (void)path; return 0; }
int file_fs_ops_file_has_bom(const char *path) { (void)path; return 0; }
char *file_fs_ops_detect_file_line_ending(const char *path) { (void)path; return strdup("\n"); }
int file_fs_ops_delete_path(const char *path) { (void)path; return 0; }
char *file_text_ops_strip_terminal_fence_leaks(const char *text) { (void)text; return strdup(""); }
char *file_text_ops_strip_bom(const char *text) { (void)text; return strdup(""); }
char *file_text_ops_parse_search_context_line(const char *line) { (void)line; return strdup("{}"); }
char *file_text_ops_normalize_line_endings(const char *text) { (void)text; return strdup(""); }
int file_text_ops_has_bom(const char *text) { (void)text; return 0; }
int file_text_ops_detect_line_ending(const char *text) { (void)text; return 0; }
int file_pagination_ops_pattern_has_regex_newline(const char *pattern) { (void)pattern; return 0; }
char *file_pagination_ops_normalize_search_pagination(const char *args) { (void)args; return strdup("{}"); }
char *file_pagination_ops_normalize_read_pagination(const char *args) { (void)args; return strdup("{}"); }
int file_pagination_ops_maybe_warn_line_oriented_newline_pattern(const char *pattern) { (void)pattern; return 0; }
int file_pagination_ops_is_line_oriented_newline_error(const char *error) { (void)error; return 0; }

/* Approval system */
int approval_permanent_allowlist_list(void) { return 0; }
int approval_load_allowlist(void) { return 0; }
const char *approval_get_current_observability_context(void) { return "{}"; }
int approval_dump_allowlist(void) { return 0; }
int approval_detect_hardline_command(void) { return 0; }
int approval_is_yolo_enabled(void) { return 0; }
int approval_parse_apply_indices(const char *indices) { (void)indices; return 0; }
int approval_load_permanent(void) { return 0; }

/* Allowlist */
int allowlist_add(const char *entry) { (void)entry; return 0; }
int allowlist_clear(void) { return 0; }
int allowlist_remove(const char *entry) { (void)entry; return 0; }
int save_permanent_allowlist(void) { return 0; }

/* Turn summary collector */
void turn_summary_collector_begin(void) {}
void turn_summary_collector_free(void *col) { free(col); }
void *turn_summary_collector_new(void) { return calloc(1, 64); }
int turn_summary_collector_record(void *col, const char *text) { (void)col; (void)text; return 0; }
char *turn_summary_collector_render(void *col) { (void)col; return strdup(""); }
char *turn_summary_format_token_flow(void) { return strdup(""); }

/* Misc */
int voice_recorder_stop(void) { return 0; }
int voice_close_stream_with_timeout(void) { return 0; }
int verify_manifest_path(void) { return 0; }
int tts_strip_markdown(void) { return 0; }
char *tf_relative_time(void) { return strdup(""); }
int system_prompt_build(void) { return 0; }
int stop_playback(void) { return 0; }
int skill_bundles_scan(void) { return 0; }
int sched_create_job_with_scheduler_registration(void) { return 0; }
const char *sbd_bundles_dir(void) { return ""; }
int relay_lease_session_id(void) { return 0; }
char *red_mask_secret(void) { return strdup("****"); }
void recipe_free(void *r) { free(r); }
int reasoning_parse_effort(void) { return 0; }
int psize_compute_toolsets_breakdown(void) { return 0; }
int psize_compute_skills_breakdown(void) { return 0; }
char *provider_normalize_custom_name(const char *name) { return name ? strdup(name) : strdup(""); }
int profile_validate_name(const char *name) { (void)name; return 1; }
char *process_registry_list(void) { return strdup("[]"); }
char *plug_get_commands(void) { return strdup("[]"); }
int plug_ensure_discovered(void) { return 0; }
int patch_apply_v4a(void) { return 0; }
char *memory_format_snapshot(void) { return strdup(""); }
char *load_skill_payload(const char *name) { (void)name; return strdup("{}"); }
char *load_pool(void) { return strdup("{}"); }
int ipx_installed(void) { return 0; }
const char *ipx_find_iron_proxy(void) { return NULL; }
const char *igr_get_provider(void) { return NULL; }
int igp_is_available(void) { return 0; }
int hub_validate_skill_name(const char *name) { (void)name; return 1; }
int hermes_state_retag_kanban_worker_sessions(void) { return 0; }
const char *hermes_state_default_db_path(void) { return ""; }
void *hermes_state_db_open(const char *path) { (void)path; return NULL; }
int hermes_state_db_close(void *db) { (void)db; return 0; }
int hermes_gateway_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
int hermes_cron_main(int argc, char **argv) { (void)argc; (void)argv; return 0; }
int hermes_cli_browser_connect_try_launch_chrome_debug(void) { return 0; }
char *google_oauth_get_valid_token(void) { return strdup(""); }
char *get_custom_provider_pool_key(void) { return strdup(""); }
int get_approval_mode(void) { return 0; }
int config_py_require_readable_config_before_write(void) { return 0; }
int config_py_is_container(void) { return 0; }
char *config_py_get_env_value_prefer_dotenv(const char *key) { (void)key; return strdup(""); }
char *config_py_get_compatible_custom_providers(void) { return strdup("[]"); }
char *cli_hermes_cli_voice_format_voice_record_key_for_status(void) { return strdup(""); }
char *cdp_get_url(void) { return strdup(""); }
char *build_proposals(void) { return strdup("[]"); }
int build_billing_state(void) { return 0; }
char *banner_resolve_repo_dir(void) { return strdup(""); }
int estop_paused_reply(void) { return 0; }
int estop_is_engaged(void) { return 0; }
int emm_cleanup(void) { return 0; }
int elc_cleanup(void) { return 0; }
int delegate_handler(void) { return 0; }
int cub_is_available(void) { return 0; }
int _save_entries(void) { return 0; }
int yuanbao_send_dm(void) { return 0; }
int file_text_ops_add_line_numbers(void) { return 0; }

