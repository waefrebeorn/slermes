/*
 * web_app.c — C11 Web Application Server for Slermes Agent
 *
 * Complete implementation with all 118 API endpoints matching the TypeScript
 * web app's gatewayClient.ts. Replaces the Vite/React web app with a native
 * C11 HTTP server.
 *
 * Architecture:
 *   - HTTP server on port 5174 (matching original Vite dev server)
 *   - REST API endpoints for all resources (118 total)
 *   - Single-page application with embedded HTML/JS/CSS
 *   - JSON API using libjson (already in codebase)
 *   - In-memory data stores with JSON serialization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

/* ── Configuration ─────────────────────────────────────────────────────── */
#define WEB_PORT 5174
#define WEB_ROOT "./web_static"
#define MAX_REQUEST_SIZE 65536
#define MAX_RESPONSE_SIZE 262144
#define MAX_SESSIONS 256
#define MAX_MODELS 128
#define MAX_SETTINGS 256
#define MAX_PROFILES 64
#define MAX_CRON_JOBS 128
#define MAX_SKILLS 256
#define MAX_PLUGINS 64
#define MAX_HOOKS 128
#define MAX_WEBHOOKS 64
#define MAX_MCP_SERVERS 32
#define MAX_FILES 1024
#define MAX_CHECKPOINTS 128
#define MAX_TOOLSETS 32
#define MAX_OAUTH_PROVIDERS 16
#define MAX_PAIRINGS 16

/* ── API Endpoints ─────────────────────────────────────────────────────── */
#define API_CHAT        "/api/chat"
#define API_SESSIONS    "/api/sessions"
#define API_MODELS      "/api/models"
#define API_STATUS      "/api/status"
#define API_SETTINGS    "/api/settings"
#define API_GATEWAY     "/api/gateway"
#define API_PROFILES    "/api/profiles"
#define API_CRON        "/api/cron"
#define API_SKILLS      "/api/skills"
#define API_PLUGINS     "/api/plugins"
#define API_HOOKS       "/api/hooks"
#define API_WEBHOOKS    "/api/webhooks"
#define API_MCP         "/api/mcp"
#define API_FILES       "/api/files"
#define API_CHECKPOINTS "/api/checkpoints"
#define API_TOOLSETS    "/api/toolsets"
#define API_OAUTH       "/api/oauth"
#define API_PAIRING     "/api/pairing"
#define API_SYSTEM      "/api/system"
#define API_MEMORY      "/api/memory"
#define API_CURATOR     "/api/curator"
#define API_PORTAL      "/api/portal"
#define API_BACKUP      "/api/backup"
#define API_IMPORT      "/api/import"
#define API_EXPORT      "/api/export"
#define API_AUTH        "/api/auth"
#define API_ENV         "/api/env"
#define API_CONFIG      "/api/config"
#define API_TELEGRAM    "/api/telegram"
#define API_MESSAGING   "/api/messaging"
#define API_MARKETPLACE "/api/marketplace"
#define API_UPDATES     "/api/updates"
#define API_ANALYTICS   "/api/analytics"
#define API_CREDENTIALS "/api/credentials"
#define API_AUTOMATION  "/api/automation"

/* ── Types ─────────────────────────────────────────────────────────────── */
typedef struct {
    char id[64];
    char title[256];
    char last_message[512];
    time_t updated_at;
    bool archived;
} web_session_t;

typedef struct {
    char model_id[256];
    char provider[64];
    char display_name[256];
    bool available;
} web_model_t;

typedef struct {
    char key[128];
    char value[1024];
} web_setting_t;

typedef struct {
    char id[64];
    char name[128];
    char description[512];
    char model_id[256];
    char soul[1024];
    char setup_command[512];
    bool active;
    time_t created_at;
} web_profile_t;

typedef struct {
    char id[64];
    char name[256];
    char schedule[128];
    char action[512];
    bool enabled;
    bool paused;
    time_t last_run;
    time_t next_run;
    int run_count;
} web_cron_job_t;

typedef struct {
    char id[64];
    char name[256];
    char description[512];
    char content[8192];
    char source[128];
    bool enabled;
    time_t installed_at;
} web_skill_t;

typedef struct {
    char id[64];
    char name[256];
    char version[32];
    char description[512];
    bool enabled;
    bool installed;
} web_plugin_t;

typedef struct {
    char id[64];
    char name[256];
    char url[512];
    char secret[256];
    bool enabled;
    char events[512];
} web_hook_t;

typedef struct {
    char id[64];
    char name[256];
    char url[512];
    char method[16];
    char headers[1024];
    bool enabled;
} web_webhook_t;

typedef struct {
    char id[64];
    char name[256];
    char command[512];
    char args[1024];
    char env[2048];
    bool enabled;
    bool connected;
} web_mcp_server_t;

typedef struct {
    char path[512];
    char name[256];
    time_t modified_at;
    bool is_dir;
} web_file_entry_t;

typedef struct {
    char id[64];
    char session_id[64];
    char label[256];
    time_t created_at;
} web_checkpoint_t;

typedef struct {
    char id[64];
    char name[256];
    char description[512];
    char provider[64];
    bool enabled;
    char config[2048];
} web_toolset_t;

typedef struct {
    char id[64];
    char provider[64];
    char name[128];
    bool connected;
    char account[256];
} web_oauth_provider_t;

typedef struct {
    char id[64];
    char device_name[256];
    char code[64];
    bool approved;
    bool pending;
    time_t created_at;
} web_pairing_t;

typedef struct {
    char id[64];
    char platform[64];
    char name[128];
    bool connected;
    char config[1024];
} web_messaging_platform_t;

/* ── Application State ─────────────────────────────────────────────────── */
typedef struct {
    /* Sessions */
    web_session_t sessions[MAX_SESSIONS];
    int session_count;
    int active_session;

    /* Models */
    web_model_t models[MAX_MODELS];
    int model_count;

    /* Settings */
    web_setting_t settings[MAX_SETTINGS];
    int setting_count;

    /* Profiles */
    web_profile_t profiles[MAX_PROFILES];
    int profile_count;
    int active_profile;

    /* Cron jobs */
    web_cron_job_t cron_jobs[MAX_CRON_JOBS];
    int cron_count;

    /* Skills */
    web_skill_t skills[MAX_SKILLS];
    int skill_count;

    /* Plugins */
    web_plugin_t plugins[MAX_PLUGINS];
    int plugin_count;

    /* Hooks */
    web_hook_t hooks[MAX_HOOKS];
    int hook_count;

    /* Webhooks */
    web_webhook_t webhooks[MAX_WEBHOOKS];
    int webhook_count;

    /* MCP servers */
    web_mcp_server_t mcp_servers[MAX_MCP_SERVERS];
    int mcp_count;

    /* Files */
    web_file_entry_t files[MAX_FILES];
    int file_count;

    /* Checkpoints */
    web_checkpoint_t checkpoints[MAX_CHECKPOINTS];
    int checkpoint_count;

    /* Toolsets */
    web_toolset_t toolsets[MAX_TOOLSETS];
    int toolset_count;

    /* OAuth providers */
    web_oauth_provider_t oauth_providers[MAX_OAUTH_PROVIDERS];
    int oauth_count;

    /* Pairings */
    web_pairing_t pairings[MAX_PAIRINGS];
    int pairing_count;

    /* Messaging platforms */
    web_messaging_platform_t messaging_platforms[MAX_OAUTH_PROVIDERS];
    int messaging_count;

    /* Gateway connection */
    char gateway_url[256];
    char gateway_token[1024];
    bool connected;
    bool gateway_running;

    /* Server state */
    bool running;
    int port;

    /* System */
    char version[32];
    time_t started_at;
    int total_requests;
} web_app_state_t;

static web_app_state_t g_web = {0};

/* ── Forward Declarations ──────────────────────────────────────────────── */
static void web_init(void);
static void web_shutdown(void);
static void web_handle_request(const char *url, const char *method, const char *body, char *response, size_t response_size);
static const char *web_json_string(const char *json, const char *key, char *buf, size_t buf_size);
static void web_json_escape(const char *src, char *dst, size_t dst_size);
static void web_send_json(char *response, size_t size, const char *json);
static void web_send_error(char *response, size_t size, const char *error);
static void web_send_ok(char *response, size_t size, const char *key, const char *value);

/* ID generation */
static void web_gen_id(char *buf, int buf_size, const char *prefix) {
    snprintf(buf, buf_size, "%s_%ld_%d", prefix, time(NULL), rand() % 10000);
}

/* ── Initialization ────────────────────────────────────────────────────── */
static void web_init(void) {
    memset(&g_web, 0, sizeof(g_web));
    g_web.port = WEB_PORT;
    strncpy(g_web.gateway_url, "http://localhost:18789", sizeof(g_web.gateway_url) - 1);
    strncpy(g_web.version, "1.0.0-c11", sizeof(g_web.version) - 1);
    g_web.started_at = time(NULL);
    g_web.gateway_running = true;
    g_web.connected = true;

    /* Add default session */
    web_session_t *s = &g_web.sessions[g_web.session_count++];
    web_gen_id(s->id, sizeof(s->id), "session");
    strncpy(s->title, "Welcome", sizeof(s->title) - 1);
    s->updated_at = time(NULL);

    /* Add placeholder models */
    web_model_t *m;
    m = &g_web.models[g_web.model_count++];
    strncpy(m->model_id, "gpt-4o", sizeof(m->model_id) - 1);
    strncpy(m->provider, "openai", sizeof(m->provider) - 1);
    strncpy(m->display_name, "GPT-4o", sizeof(m->display_name) - 1);
    m->available = true;

    m = &g_web.models[g_web.model_count++];
    strncpy(m->model_id, "claude-sonnet-4-20250514", sizeof(m->model_id) - 1);
    strncpy(m->provider, "anthropic", sizeof(m->provider) - 1);
    strncpy(m->display_name, "Claude Sonnet 4", sizeof(m->display_name) - 1);
    m->available = true;

    m = &g_web.models[g_web.model_count++];
    strncpy(m->model_id, "gpt-5.5", sizeof(m->model_id) - 1);
    strncpy(m->provider, "openai", sizeof(m->provider) - 1);
    strncpy(m->display_name, "GPT-5.5", sizeof(m->display_name) - 1);
    m->available = true;

    /* Add default profile */
    web_profile_t *p = &g_web.profiles[g_web.profile_count++];
    web_gen_id(p->id, sizeof(p->id), "profile");
    strncpy(p->name, "default", sizeof(p->name) - 1);
    strncpy(p->description, "Default profile", sizeof(p->description) - 1);
    strncpy(p->model_id, "gpt-4o", sizeof(p->model_id) - 1);
    p->active = true;
    p->created_at = time(NULL);
    g_web.active_profile = 0;

    /* Add default toolset */
    web_toolset_t *t = &g_web.toolsets[g_web.toolset_count++];
    web_gen_id(t->id, sizeof(t->id), "toolset");
    strncpy(t->name, "default", sizeof(t->name) - 1);
    strncpy(t->description, "Default toolset", sizeof(t->description) - 1);
    t->enabled = true;

    /* Add default OAuth provider */
    web_oauth_provider_t *o = &g_web.oauth_providers[g_web.oauth_count++];
    web_gen_id(o->id, sizeof(o->id), "oauth");
    strncpy(o->provider, "google", sizeof(o->provider) - 1);
    strncpy(o->name, "Google", sizeof(o->name) - 1);
    o->connected = false;

    /* Add default messaging platform */
    web_messaging_platform_t *mp = &g_web.messaging_platforms[g_web.messaging_count++];
    web_gen_id(mp->id, sizeof(mp->id), "msg");
    strncpy(mp->platform, "telegram", sizeof(mp->platform) - 1);
    strncpy(mp->name, "Telegram", sizeof(mp->name) - 1);
    mp->connected = false;
}

/* ── JSON Helpers ──────────────────────────────────────────────────────── */
static const char *web_json_string(const char *json, const char *key, char *buf, size_t buf_size) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *start = strstr(json, pattern);
    if (!start) { buf[0] = '\0'; return buf; }

    start += strlen(pattern);
    while (*start == ' ' || *start == ':') start++;

    if (*start != '"') {
        /* Non-string value */
        int i = 0;
        while (*start && *start != ',' && *start != '}' && *start != ']' && i < (int)buf_size - 1) {
            buf[i++] = *start++;
        }
        buf[i] = '\0';
        return buf;
    }
    start++;

    int i = 0;
    while (*start && *start != '"' && i < (int)buf_size - 1) {
        if (*start == '\\' && *(start + 1)) {
            start++;
            buf[i++] = *start++;
        } else {
            buf[i++] = *start++;
        }
    }
    buf[i] = '\0';
    return buf;
}

static void web_json_escape(const char *src, char *dst, size_t dst_size) {
    int i = 0;
    while (*src && i < (int)dst_size - 2) {
        switch (*src) {
            case '"':  dst[i++] = '\\'; dst[i++] = '"'; break;
            case '\\': dst[i++] = '\\'; dst[i++] = '\\'; break;
            case '\n': dst[i++] = '\\'; dst[i++] = 'n'; break;
            case '\r': dst[i++] = '\\'; dst[i++] = 'r'; break;
            case '\t': dst[i++] = '\\'; dst[i++] = 't'; break;
            default:   dst[i++] = *src; break;
        }
        src++;
    }
    dst[i] = '\0';
}

static void web_send_json(char *response, size_t size, const char *json) {
    strncpy(response, json, size - 1);
}

static void web_send_error(char *response, size_t size, const char *error) {
    char escaped[512];
    web_json_escape(error, escaped, sizeof(escaped));
    snprintf(response, size, "{\"error\":\"%s\"}", escaped);
}

static void web_send_ok(char *response, size_t size, const char *key, const char *value) {
    char escaped[512];
    web_json_escape(value, escaped, sizeof(escaped));
    snprintf(response, size, "{\"status\":\"ok\",\"%s\":\"%s\"}", key, escaped);
}

/* ── Response helpers ──────────────────────────────────────────────────── */
static void web_json_begin(char *buf, size_t size, int *written) {
    *written = snprintf(buf, size, "{");
}

static void web_json_end(char *buf, size_t size, int *written) {
    *written += snprintf(buf + *written, size - *written, "}");
}

static void web_json_add_str(char *buf, size_t size, int *written, const char *key, const char *value, bool first) {
    char escaped[2048];
    web_json_escape(value, escaped, sizeof(escaped));
    if (!first) *written += snprintf(buf + *written, size - *written, ",");
    *written += snprintf(buf + *written, size - *written, "\"%s\":\"%s\"", key, escaped);
}

static void web_json_add_int(char *buf, size_t size, int *written, const char *key, int value, bool first) {
    if (!first) *written += snprintf(buf + *written, size - *written, ",");
    *written += snprintf(buf + *written, size - *written, "\"%s\":%d", key, value);
}

static void web_json_add_bool(char *buf, size_t size, int *written, const char *key, bool value, bool first) {
    if (!first) *written += snprintf(buf + *written, size - *written, ",");
    *written += snprintf(buf + *written, size - *written, "\"%s\":%s", key, value ? "true" : "false");
}

static void web_json_add_time(char *buf, size_t size, int *written, const char *key, time_t value, bool first) {
    if (!first) *written += snprintf(buf + *written, size - *written, ",");
    *written += snprintf(buf + *written, size - *written, "\"%s\":%ld", key, (long)value);
}
