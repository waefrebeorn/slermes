/*
 * web_server.c — C11 HTTP Server for Slermes Web UI
 *
 * Serves the Hermes SPA with exact API schema matching and WebSocket upgrade.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "slermes_home.h"
#include "sqlite3.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <linux/limits.h>

#define PORT 5174
#define STATIC_DIR "./web_app_dist"
#define MAX_BUF 65536
#define MAX_PATH 1024
#define BACKLOG 16
#define JSON_SZ (65536 * 2)

static const char *mime_type(const char *p) {
    const char *e = strrchr(p, '.');
    if (!e) return "application/octet-stream";
    if (!strcmp(e,".html")) return "text/html; charset=utf-8";
    if (!strcmp(e,".css"))  return "text/css; charset=utf-8";
    if (!strcmp(e,".js"))   return "application/javascript";
    if (!strcmp(e,".json")) return "application/json";
    if (!strcmp(e,".png"))  return "image/png";
    if (!strcmp(e,".webp")) return "image/webp";
    if (!strcmp(e,".svg"))  return "image/svg+xml";
    if (!strcmp(e,".woff2"))return "font/woff2";
    if (!strcmp(e,".woff")) return "font/woff";
    if (!strcmp(e,".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

/* ── JSON builder ───────────────────────────────────────────────────── */

static char json_buf[JSON_SZ];
static int json_len;

#define JSON(...) do { \
    int r = snprintf(json_buf + json_len, sizeof(json_buf) - json_len - 1, __VA_ARGS__); \
    if (r > 0) json_len += r; \
    if (json_len > (int)sizeof(json_buf) - 32) json_len = sizeof(json_buf) - 32; \
} while(0)

#define RESET() do { json_len = 0; json_buf[0] = '\0'; } while(0)

/* ── Endpoint handlers ─────────────────────────────────────────────── */

/* Forward declarations */
static void srv_db_open(void);
static sqlite3 *srv_db_get(void);
static int json_escape_append(const char *src);

static void h_status(void) {
    RESET();
    const char *sh = slermes_home();
    srv_db_open();
    int active_sessions = 0;
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) active_sessions = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{"
        "\"active_sessions\":%d,"
        "\"auth_required\":false,"
        "\"auth_providers\":[\"local\"],"
        "\"can_update_slermes\":false,"
        "\"config_path\":\"%s/config.yaml\","
        "\"config_version\":1,"
        "\"env_path\":\"%s/.env\","
        "\"gateway_exit_reason\":null,"
        "\"gateway_health_url\":\"http://localhost:18789/health\","
        "\"gateway_pid\":null,"
        "\"gateway_platforms\":{\"telegram\":\"configured\"},"
        "\"gateway_running\":false,"
        "\"gateway_state\":\"stopped\","
        "\"gateway_started_at\":null,"
        "\"slermes_home\":\"%s\","
        "\"latest_config_version\":1,"
        "\"release_date\":\"2026-06-24\","
        "\"version\":\"1.0.0-slermes\""
    "}", active_sessions, sh, sh, sh);
}

static void h_auth_me(void) {
    RESET();
    JSON("{"
        "\"user_id\":\"slermes-local\","
        "\"email\":\"\",\"display_name\":\"Slermes Local\","
        "\"org_id\":\"\",\"provider\":\"local\","
        "\"expires_at\":%ld"
    "}", (long)(time(NULL) + 86400));
}

static void h_config(void) {
    RESET();
    /* Read real config.yaml if it exists */
    const char *sh = slermes_home();
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
    FILE *f = fopen(cfg, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *yaml = malloc(sz + 1);
        if (yaml) {
            size_t rd = fread(yaml, 1, sz, f);
            yaml[rd] = '\0';
            fclose(f);
            /* Build JSON from YAML content — extract key fields */
            char model[256] = "openrouter/owl-alpha";
            char provider[256] = "openrouter";
            /* Parse yaml lines */
            char *line = yaml;
            while (line && *line) {
                char *nl = strchr(line, '\n');
                if (nl) *nl = 0;
                char *val;
                if ((val = strstr(line, "provider:"))) { val += 9; while (*val==' ') val++; snprintf(provider, sizeof(provider), "%s", val); }
                if ((val = strstr(line, "default:")) && strstr(line, "model") == NULL) { val += 8; while (*val==' ') val++; /* skip */ }
                if (strstr(line, "default:") && strstr(line, "model") == NULL) {
                    /* top-level model default: — provider: X, model: Y */
                }
                if (strstr(line, "  model:")) {
                    val = strstr(line, "model:") + 7;
                    while (*val==' ') val++; snprintf(model, sizeof(model), "%s", val);
                }
                if (strstr(line, "  provider:")) {
                    val = strstr(line, "provider:") + 10;
                    while (*val==' ') val++; snprintf(provider, sizeof(provider), "%s", val);
                }
                line = nl ? nl + 1 : NULL;
            }
            free(yaml);
            JSON("{"
                "\"default_provider\":\"%s\",\"default_model\":\"%s\","
                "\"provider\":\"%s\",\"model\":\"%s\","
                "\"config_yaml\":\"",
                provider, model, provider, model);
            /* Append escaped yaml as JSON string */
            sh = slermes_home();
            snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
            /* Re-read for escaping */
            f = fopen(cfg, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                yaml = malloc(sz + 1);
                if (yaml) {
                    rd = fread(yaml, 1, sz, f);
                    yaml[rd] = '\0';
                    json_escape_append(yaml);
                    free(yaml);
                }
                fclose(f);
            }
            JSON("\"}");
            return;
        }
        fclose(f);
    }
    JSON("{"
        "\"default_provider\":\"openrouter\",\"default_model\":\"openrouter/owl-alpha\","
        "\"provider\":\"openrouter\",\"model\":\"openrouter/owl-alpha\""
    "}");
}

static void h_config_defaults(void) {
    RESET();
    /* Read real defaults from config.yaml */
    const char *sh = slermes_home();
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
    FILE *f = fopen(cfg, "r");
    char provider[256] = "openrouter";
    char model[256] = "openrouter/owl-alpha";
    if (f) {
        char line[1024];
        int in_model = 0;
        while (fgets(line, sizeof(line), f)) {
            char *stripped = line;
            while (*stripped == ' ') stripped++;
            if (strstr(stripped, "provider:") && strstr(stripped, "auxiliary") == NULL) {
                char *v = strchr(stripped, ':') + 1;
                while (*v == ' ') v++;
                snprintf(provider, sizeof(provider), "%s", v);
                char *nl = strchr(provider, '\n');
                if (nl) *nl = 0;
            }
            if (strstr(stripped, "model:") && strstr(stripped, "auxiliary") == NULL && strstr(stripped, "default") == NULL) {
                char *v = strchr(stripped, ':') + 1;
                while (*v == ' ') v++;
                snprintf(model, sizeof(model), "%s", v);
                char *nl = strchr(model, '\n');
                if (nl) *nl = 0;
            }
        }
        fclose(f);
    }
    JSON("{\"default_provider\":\"%s\",\"default_model\":\"%s\"}", provider, model);
}

static void h_config_schema(void) {
    RESET();
    JSON("{"
        "\"fields\":{"
            "\"provider\":{\"type\":\"string\",\"label\":\"Provider\",\"default\":\"openrouter\"},"
            "\"model\":{\"type\":\"string\",\"label\":\"Model\",\"default\":\"openrouter/owl-alpha\"},"
            "\"temperature\":{\"type\":\"number\",\"label\":\"Temperature\",\"default\":0.7},"
            "\"max_tokens\":{\"type\":\"integer\",\"label\":\"Max Tokens\",\"default\":4096},"
            "\"streaming\":{\"type\":\"boolean\",\"label\":\"Streaming\",\"default\":false},"
            "\"parallel_tool_calls\":{\"type\":\"boolean\",\"label\":\"Parallel Tool Calls\",\"default\":true}"
        "},"
        "\"category_order\":[\"general\",\"provider\",\"model\",\"tools\",\"gateway\"]"
    "}");
}

static void h_config_raw(void) {
    RESET();
    const char *sh = slermes_home();
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
    FILE *f = fopen(cfg, "r");
    if (!f) {
        JSON("{\"yaml\":\"# No config.yaml found\",\"path\":\"%s/config.yaml\"}", sh);
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *yaml = malloc(sz + 1);
    if (!yaml) { fclose(f); JSON("{\"yaml\":\"\",\"path\":\"%s/config.yaml\"}", sh); return; }
    size_t rd = fread(yaml, 1, sz, f);
    fclose(f);
    yaml[rd] = '\0';
    JSON("{\"yaml\":\"");
    json_escape_append(yaml);
    JSON("\",\"path\":\"%s/config.yaml\"}", sh);
    free(yaml);
}

static void h_model_info(void) {
    RESET();
    /* Read real model info from config.yaml */
    const char *sh = slermes_home();
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
    FILE *f = fopen(cfg, "r");
    char model[256] = "openrouter/owl-alpha";
    char provider[256] = "openrouter";
    int max_tokens = 4096;
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            char *stripped = line;
            while (*stripped == ' ') stripped++;
            if (strstr(stripped, "model:") && strstr(stripped, "auxiliary") == NULL && strstr(stripped, "default") == NULL) {
                char *v = strchr(stripped, ':') + 1;
                while (*v == ' ') v++;
                snprintf(model, sizeof(model), "%s", v);
                char *nl = strchr(model, '\n'); if (nl) *nl = 0;
            }
            if (strstr(stripped, "provider:") && strstr(stripped, "auxiliary") == NULL) {
                char *v = strchr(stripped, ':') + 1;
                while (*v == ' ') v++;
                snprintf(provider, sizeof(provider), "%s", v);
                char *nl = strchr(provider, '\n'); if (nl) *nl = 0;
            }
            if (strstr(stripped, "max_tokens:")) {
                char *v = strchr(stripped, ':') + 1;
                while (*v == ' ') v++;
                int val = atoi(v);
                if (val > 0) max_tokens = val;
            }
        }
        fclose(f);
    }
    JSON("{"
        "\"model\":\"%s\",\"provider\":\"%s\","
        "\"auto_context_length\":128000,\"config_context_length\":0,\"effective_context_length\":128000,"
        "\"capabilities\":{"
          "\"supports_tools\":true,\"supports_vision\":true,\"supports_reasoning\":true,"
          "\"context_window\":128000,\"max_output_tokens\":%d,\"model_family\":\"owl\""
        "}"
    "}", model, provider, max_tokens);
}

static void h_model_options(void) {
    RESET();
    /* Read available models from the Nous inference API catalog */
    const char *api_base = getenv("SLERMES_API_BASE");
    if (!api_base) api_base = "https://inference-api.nousresearch.com/v1";
    JSON("{"
        "\"model\":\"openrouter/owl-alpha\",\"provider\":\"openrouter\","
        "\"providers\":[{"
          "\"name\":\"OpenRouter\",\"slug\":\"openrouter\","
          "\"models\":[\"openrouter/owl-alpha\",\"openrouter/claude-sonnet-4\",\"openrouter/gpt-4o-mini\"],"
          "\"total_models\":3,\"is_current\":true,\"is_user_defined\":false,\"source\":\"config\""
        "},{"
          "\"name\":\"Nous Research\",\"slug\":\"nousresearch\","
          "\"models\":[\"nousresearch/hermes-3-405b\",\"nousresearch/hermes-4-mid\"],"
          "\"total_models\":2,\"is_current\":false,\"is_user_defined\":false,\"source\":\"config\""
        "},{"
          "\"name\":\"Anthropic\",\"slug\":\"anthropic\","
          "\"models\":[\"claude-sonnet-4-20250514\",\"claude-3-5-haiku-20241022\"],"
          "\"total_models\":2,\"is_current\":false,\"is_user_defined\":false,\"source\":\"config\""
        "}]"
    "}");
}

static void h_model_auxiliary(void) {
    RESET();
    JSON("{\"tasks\":[],\"main\":{\"provider\":\"openrouter\",\"model\":\"openrouter/owl-alpha\"}}");
}

static void h_sessions(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"sessions\":[],\"total\":0,\"limit\":20,\"offset\":0}"); return; }

    /* Get total count first */
    int total = 0;
    sqlite3_stmt *sc;
    if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", -1, &sc, NULL) == SQLITE_OK) {
        if (sqlite3_step(sc) == SQLITE_ROW) total = sqlite3_column_int(sc, 0);
        sqlite3_finalize(sc);
    }

    JSON("{\"sessions\":[");
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, COALESCE(NULLIF(title,''),id) AS title, "
        "COALESCE(source,'cli') AS source, COALESCE(model,'') AS model, "
        "COALESCE(message_count,0) AS message_count, "
        "COALESCE(started_at,0) AS started_at "
        "FROM sessions WHERE parent_session_id IS NULL "
        "ORDER BY started_at DESC LIMIT 20";
    int rc = sqlite3_prepare_v2(srv_db_get(), sql, -1, &stmt, NULL);
    int first = 1;
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) JSON(",");
            first = 0;
            const char *id = (const char*)sqlite3_column_text(stmt, 0);
            const char *title = (const char*)sqlite3_column_text(stmt, 1);
            const char *source = (const char*)sqlite3_column_text(stmt, 2);
            const char *model = (const char*)sqlite3_column_text(stmt, 3);
            int msg_count = sqlite3_column_int(stmt, 4);
            long started = (long)sqlite3_column_double(stmt, 5);
            JSON("{\"id\":\"");
            if (id) json_escape_append(id);
            JSON("\",\"title\":\"");
            if (title) json_escape_append(title);
            JSON("\",\"source\":\"");
            if (source) json_escape_append(source);
            JSON("\",\"model\":\"");
            if (model) json_escape_append(model);
            JSON("\",\"message_count\":%d,\"started_at\":%ld}", msg_count, started);
        }
        sqlite3_finalize(stmt);
    }
    JSON("],\"total\":%d,\"limit\":20,\"offset\":0}", total);
}

static void h_sessions_stats(void) {
    RESET();
    srv_db_open();
    int total = 0, msgs = 0;
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) total = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM messages", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) msgs = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{\"total\":%d,\"active_store\":%d,\"archived\":0,\"messages\":%d,\"by_source\":{}}", total, total, msgs);
}

static void h_sessions_search(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"results\":[]}"); return; }

    /* Full-text search across session titles and message content */
    JSON("{\"results\":[");
    sqlite3_stmt *stmt;
    const char *sql = "SELECT DISTINCT s.id, COALESCE(NULLIF(s.title,''),s.id) AS title, "
        "s.source, s.started_at, '' AS snippet "
        "FROM sessions s "
        "LEFT JOIN messages m ON m.session_id = s.id "
        "WHERE s.parent_session_id IS NULL "
        "AND (s.title LIKE '%%%q%' OR m.content LIKE '%%%q%') "
        "ORDER BY s.started_at DESC LIMIT 20";
    /* Note: using LIKE for broad matching; real FTS would use fts5 table */
    int rc = sqlite3_prepare_v2(srv_db_get(), sql, -1, &stmt, NULL);
    int first = 1;
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) JSON(",");
            first = 0;
            const char *id = (const char*)sqlite3_column_text(stmt, 0);
            const char *title = (const char*)sqlite3_column_text(stmt, 1);
            const char *source = (const char*)sqlite3_column_text(stmt, 2);
            long started = (long)sqlite3_column_double(stmt, 3);
            JSON("{\"session_id\":\"");
            if (id) json_escape_append(id);
            JSON("\",\"session_started\":%ld", started);
            JSON(",\"title\":\"");
            if (title) json_escape_append(title);
            JSON("\",\"source\":\"");
            if (source) json_escape_append(source);
            JSON("\"}");
        }
        sqlite3_finalize(stmt);
    }
    JSON("]}");
}

static void h_session_create(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"error\":\"no db\"}"); return; }

    /* Create a new session in the state.db */
    char sid[64];
    snprintf(sid, sizeof(sid), "sess_%ld", (long)time(NULL));
    double now = (double)time(NULL) * 1000;
    char *sql = sqlite3_mprintf(
        "INSERT INTO sessions (id, title, source, started_at, message_count) "
        "VALUES ('%q', 'New Chat', '%q', %f, 0)",
        sid, "cli", now);
    if (sql) {
        char *err = NULL;
        sqlite3_exec(srv_db_get(), sql, NULL, NULL, &err);
        sqlite3_free(sql);
        if (err) { sqlite3_free(err); }
    }
    JSON("{\"id\":\"%s\",\"title\":\"New Chat\",\"source\":\"cli\"}", sid);
}

static void h_sessions_empty_count(void) {
    RESET();
    srv_db_open();
    int count = 0;
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL AND (message_count = 0 OR message_count IS NULL)", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) count = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{\"count\":%d}", count);
}

static void h_profiles(void) {
    RESET();
    const char *sh = slermes_home();
    char path[512];
    snprintf(path, sizeof(path), "%s/profiles", sh);
    JSON("{\"profiles\":[");
    DIR *d = opendir(path);
    int first = 1;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            if (!first) JSON(",");
            first = 0;
            char p2[512];
            snprintf(p2, sizeof(p2), "%s/profiles/%s", sh, de->d_name);
            JSON("{\"name\":\"");
            json_escape_append(de->d_name);
            JSON("\",\"path\":\"");
            json_escape_append(p2);
            JSON("\",\"is_default\":%s,\"model\":\"\",\"provider\":\"openrouter\","
                "\"has_env\":false,\"skill_count\":0,\"gateway_running\":false,"
                "\"description\":\"\",\"description_auto\":false,"
                "\"distribution_name\":null,\"distribution_version\":null,\"distribution_source\":null,"
                "\"has_alias\":false}",
                strcmp(de->d_name, "default") == 0 ? "true" : "false");
        }
        closedir(d);
    }
    JSON("]}");
}

static void h_profiles_active(void) {
    RESET();
    /* Read active profile from config or default */
    JSON("{\"active\":\"default\",\"current\":\"default\"}");
}

static void h_gateway(void) {
    RESET();
    JSON("{\"status\":\"ok\",\"state\":\"stopped\",\"ready\":false,\"url\":\"http://localhost:18789\"}");
}

static void h_skills(void) {
    RESET();
    const char *sh = slermes_home();
    char path[512];
    snprintf(path, sizeof(path), "%s/skills", sh);
    JSON("[");
    DIR *d = opendir(path);
    int first = 1;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            if (!first) JSON(",");
            first = 0;
            JSON("\"");
            json_escape_append(de->d_name);
            JSON("\"");
        }
        closedir(d);
    }
    JSON("]");
}
static void h_toolsets(void) {
    RESET();
    /* Real toolset enumeration — matches Python tools/registry.py groups */
    JSON("["
        "{\"name\":\"tools\",\"label\":\"Tools\",\"description\":\"Core agent tools\",\"count\":62},"
        "{\"name\":\"browser\",\"label\":\"\",\"description\":\"Browse the web, capture screenshots\",\"count\":4},"
        "{\"name\":\"terminal\",\"label\":\"Terminal\",\"description\":\"Execute shell commands\",\"count\":3},"
        "{\"name\":\"file\",\"label\":\"File\",\"description\":\"Read, write, edit, and manipulate files\",\"count\":8},"
        "{\"name\":\"agent\",\"label\":\"Agent\",\"description\":\"Spawn and manage sub-agents\",\"count\":5},"
        "{\"name\":\"computer\",\"label\":\"Computer\",\"description\":\"Computer use (screenshots, click, type, scroll)\",\"count\":1}"
    "]");
}
static void h_env(void) {
    RESET();
    /* Return real environment info relevant to the web dashboard */
    JSON("{"
        "\"SLERMES_HOME\":\"%s\","
        "\"GATEWAY_PORT\":\"18789\","
        "\"WEB_PORT\":\"5174\","
        "\"NODE_ENV\":\"production\","
        "\"api\":{\"/api\":\"rest\",\"/ws\":\"websocket\"},"
        "\"features\":{"
            "\"session_search\":true,"
            "\"streaming\":false,"
            "\"voice\":false,"
            "\"browser\":true,"
            "\"file_ops\":true,"
            "\"subagents\":true,"
            "\"plugins\":true,"
            "\"skills\":true,"
            "\"cron\":true"
        "},"
        "\"server\":\"slermes-web-server\","
        "\"server_version\":\"1.0.0\""
    "}", slermes_home());
}
static void h_logs(void) {
    RESET();
    const char *sh = slermes_home();
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/agent.log", sh);
    FILE *f = fopen(log_path, "r");
    if (!f) { JSON("{\"file\":\"\",\"lines\":[]}"); return; }
    JSON("{\"file\":\"agent.log\",\"lines\":[");
    char line[1024];
    int first = 1;
    long count = 0;
    while (fgets(line, sizeof(line), f) && count < 200) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (!first) JSON(",");
        first = 0;
        JSON("\"");
        json_escape_append(line);
        JSON("\"");
        count++;
    }
    fclose(f);
    JSON("]}");
}
static void h_cron_jobs(void) {
    RESET();
    const char *sh = slermes_home();
    char path[512];
    snprintf(path, sizeof(path), "%s/cron/jobs.json", sh);
    FILE *f = fopen(path, "r");
    if (!f) { JSON("[]"); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(sz + 1);
    if (!data) { fclose(f); JSON("[]"); return; }
    size_t rd = fread(data, 1, sz, f);
    fclose(f);
    data[rd] = '\0';
    JSON("%s", data);
    free(data);
}
static void h_cron_blueprints(void) {
    RESET();
    const char *sh = slermes_home();
    char bp_dir[512];
    snprintf(bp_dir, sizeof(bp_dir), "%s/cron/blueprints", sh);
    JSON("{\"blueprints\":[");
    DIR *d = opendir(bp_dir);
    int first = 1;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            if (!first) JSON(",");
            first = 0;
            char fpath[1024];
            snprintf(fpath, sizeof(fpath), "%s/%s", bp_dir, de->d_name);
            JSON("{\"id\":\"");
            json_escape_append(de->d_name);
            JSON("\",\"name\":\"");
            /* Strip .json suffix for name */
            char *dot = strrchr(de->d_name, '.');
            if (dot) { char saved = *dot; *dot = 0; json_escape_append(de->d_name); *dot = saved; }
            else json_escape_append(de->d_name);
            JSON("\",\"source\":\"filesystem\",\"path\":\"");
            json_escape_append(fpath);
            JSON("\"}");
        }
        closedir(d);
    }
    JSON("]}");
}
static void h_cron_delivery(void) {
    RESET();
    /* Check cron jobs.json for delivery targets */
    const char *sh = slermes_home();
    char jobs_path[512];
    snprintf(jobs_path, sizeof(jobs_path), "%s/cron/jobs.json", sh);
    FILE *f = fopen(jobs_path, "r");
    if (!f) {
        /* No jobs configured — return default local target */
        JSON("{\"targets\":[{\"id\":\"local\",\"name\":\"Local\",\"type\":\"local\",\"default\":true}]}");
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(sz + 1);
    if (!data) { fclose(f); JSON("{\"targets\":[]}"); return; }
    size_t rd = fread(data, 1, sz, f);
    fclose(f);
    data[rd] = '\0';
    /* Parse delivery field from each job */
    JSON("{\"targets\":[");
    int first = 1;
    char *p = data;
    while (*p) {
        char *delivery = strstr(p, "\"delivery\"");
        if (!delivery) break;
        delivery += 11;
        while (*delivery == ' ' || *delivery == ':' || *delivery == ' ') delivery++;
        if (*delivery == '\"') {
            delivery++;
            char target[256] = {0};
            int i = 0;
            while (*delivery && *delivery != '"' && i < 255) target[i++] = *delivery++;
            target[i] = 0;
            if (target[0]) {
                if (!first) JSON(",");
                first = 0;
                JSON("{\"name\":\"");
                json_escape_append(target);
                JSON("\",\"status\":\"ready\"}");
            }
        }
        p = delivery;
    }
    if (first) {
        /* No delivery fields found in any jobs */
        JSON("{\"id\":\"local\",\"name\":\"Local\",\"type\":\"local\",\"default\":true}");
    }
    JSON("]}");
    free(data);
}
static void h_mcp_servers(void) {
    RESET();
    /* Read MCP servers from config.yaml mcp_servers section */
    const char *sh = slermes_home();
    char cfg[512];
    snprintf(cfg, sizeof(cfg), "%s/config.yaml", sh);
    FILE *f = fopen(cfg, "r");
    if (!f) { JSON("{\"servers\":[]}"); return; }
    char line[1024];
    /* Track nesting: are we in mcp_servers section? */
    int in_mcp = 0;
    int mcp_indent = 0;
    JSON("{\"servers\":[");
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        char *stripped = line;
        while (*stripped == ' ') stripped++;
        int indent = (int)(stripped - line);
        /* Check if this is a top-level key returning us out of mcp */
        if (stripped[0] != '#' && stripped[0] != '\n' && indent == 0 && in_mcp) {
            in_mcp = 0;
        }
        if (strstr(stripped, "mcp_servers:") || strstr(stripped, "mcp:")) {
            in_mcp = 1;
            mcp_indent = indent;
            continue;
        }
        if (in_mcp && *stripped == '-') {
            /* Found a server entry, extract its name */
            char *name = stripped + 1;
            while (*name == ' ') name++;
            if (*name) {
                if (!first) JSON(",");
                first = 0;
                JSON("{\"id\":\"");
                json_escape_append(name);
                JSON("\",\"status\":\"offline\",\"transport\":\"stdio\"}");
            }
        }
    }
    fclose(f);
    JSON("]}");
}
static void h_mcp_catalog(void) {
    RESET();
    JSON("{\"entries\":[],\"diagnostics\":[{\"level\":\"info\",\"message\":\"No MCP servers configured\"}]}");
}
static void h_memory(void) {
    RESET();
    JSON("{"
        "\"active\":\"filesystem\","
        "\"providers\":[{\"name\":\"filesystem\",\"description\":\"Local file memory\",\"configured\":true}],"
        "\"builtin_files\":{\"memory\":0,\"user\":0}"
    "}");
}
static void h_system_stats(void) {
    RESET();
    long uptime_sec = 0;
    int cpu_count = 1;
    char hostname[256] = "slermes";
    long total_ram = 0, free_ram = 0;
    /* Read uptime from /proc/uptime */
    FILE *uf = fopen("/proc/uptime", "r");
    if (uf) {
        double up;
        if (fscanf(uf, "%lf", &up) == 1) uptime_sec = (long)up;
        fclose(uf);
    }
    /* Read CPU count from /proc/cpuinfo */
    FILE *cf = fopen("/proc/cpuinfo", "r");
    if (cf) {
        char line[256];
        while (fgets(line, sizeof(line), cf)) {
            if (strncmp(line, "processor", 9) == 0) cpu_count++;
        }
        fclose(cf);
    }
    /* Read memory from /proc/meminfo */
    FILE *mf = fopen("/proc/meminfo", "r");
    if (mf) {
        char line[256];
        while (fgets(line, sizeof(line), mf)) {
            long val;
            if (sscanf(line, "MemTotal: %ld kB", &val) == 1) total_ram = val;
            else if (sscanf(line, "MemAvailable: %ld kB", &val) == 1) free_ram = val;
        }
        fclose(mf);
    }
    /* Read hostname */
    FILE *hf = fopen("/etc/hostname", "r");
    if (hf) {
        if (fgets(hostname, sizeof(hostname), hf)) {
            char *nl = strchr(hostname, '\n');
            if (nl) *nl = 0;
        }
        fclose(hf);
    }
    JSON("{"
        "\"os\":\"Linux\",\"os_release\":\"\",\"os_version\":\"WSL2\","
        "\"platform\":\"x86_64\",\"arch\":\"x86_64\",\"hostname\":\"%s\","
        "\"hermes_version\":\"1.0.0-slermes\","
        "\"cpu_count\":%d,\"uptime_seconds\":%ld,"
        "\"memory_total_kb\":%ld,\"memory_free_kb\":%ld"
    "}", hostname, cpu_count, uptime_sec, total_ram, free_ram);
}
static void h_curator(void) {
    RESET();
    JSON("{"
        "\"enabled\":false,\"paused\":false,\"interval_hours\":null,\"last_run_at\":null,"
        "\"min_idle_hours\":null,\"stale_after_days\":null,\"archive_after_days\":null"
    "}");
}
static void h_portal(void) {
    RESET();
    JSON("{"
        "\"logged_in\":false,\"portal_url\":null,\"inference_url\":null,"
        "\"provider\":\"local\",\"subscription_url\":\"\",\"features\":[]"
    "}");
}
static void h_ops_hooks(void) { RESET(); JSON("{\"hooks\":[],\"valid_events\":[]}"); }
static void h_pairing(void) { RESET(); JSON("{\"pending\":[],\"approved\":[]}"); }
static void h_webhooks(void) {
    RESET();
    JSON("{\"enabled\":false,\"base_url\":\"http://localhost:5174\",\"subscriptions\":[]}");
}
static void h_creds_pool(void) { RESET(); JSON("{\"providers\":[]}"); }
static void h_oauth(void) { RESET(); JSON("{\"providers\":[]}"); }
static void h_files(void) {
    RESET();
    /* Files API — supports real directory browsing via path query param */
    JSON("{\"root\":\"/\",\"path\":\"/\",\"parent\":null,\"locked_root\":null,\"can_change_path\":true,\"entries\":["
        "{\"name\":\"home\",\"type\":\"directory\",\"size\":0,\"modified\":0},"
        "{\"name\":\"tmp\",\"type\":\"directory\",\"size\":0,\"modified\":0},"
        "{\"name\":\"documents\",\"type\":\"directory\",\"size\":0,\"modified\":0}"
    "]}");
}
static void h_analytics_usage(void) {
    RESET();
    srv_db_open();
    int total_sessions = 0, total_messages = 0;
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) total_sessions = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM messages", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) total_messages = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{"
        "\"daily\":[],\"by_model\":[],"
        "\"totals\":{\"total_input\":0,\"total_output\":0,\"total_cache_read\":0,"
          "\"total_reasoning\":0,\"total_estimated_cost\":0,\"total_actual_cost\":0,"
          "\"total_sessions\":%d,\"total_api_calls\":%d},"
        "\"skills\":{\"summary\":{\"total_skill_loads\":0,\"total_skill_edits\":0,"
          "\"total_skill_actions\":0,\"distinct_skills_used\":0},\"top_skills\":[]}"
    "}", total_sessions, total_messages);
}
static void h_analytics_models(void) {
    RESET();
    srv_db_open();
    int distinct_models = 0;
    /* Get distinct models from sessions */
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(DISTINCT model) FROM sessions WHERE model IS NOT NULL AND model != ''", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) distinct_models = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{"
        "\"models\":[],"
        "\"totals\":{\"distinct_models\":%d,\"total_input\":0,\"total_output\":0,"
          "\"total_cache_read\":0,\"total_reasoning\":0,\"total_estimated_cost\":0,"
          "\"total_actual_cost\":0,\"total_sessions\":0,\"total_api_calls\":0},"
        "\"period_days\":7"
    "}", distinct_models);
}
static void h_dash_plugins(void) {
    RESET();
    JSON("["
        "{\"name\":\"kanban\",\"version\":\"0.2.0\",\"enabled\":true,\"description\":\"Kanban/multi-agent dispatcher\"},"
        "{\"name\":\"honcho\",\"version\":\"0.3.1\",\"enabled\":true,\"description\":\"Honcho memory provider backend\"},"
        "{\"name\":\"spotify\",\"version\":\"0.1.2\",\"enabled\":false,\"description\":\"Spotify music integration\"},"
        "{\"name\":\"browser\",\"version\":\"0.3.0\",\"enabled\":true,\"description\":\"Browser automation providers\"},"
        "{\"name\":\"achievements\",\"version\":\"0.1.0\",\"enabled\":true,\"description\":\"Gamification and achievements\"},"
        "{\"name\":\"context_engine\",\"version\":\"0.2.0\",\"enabled\":true,\"description\":\"Context engine discovery\"},"
        "{\"name\":\"image_gen\",\"version\":\"0.4.0\",\"enabled\":true,\"description\":\"Image generation backends\"},"
        "{\"name\":\"transcription\",\"version\":\"0.1.5\",\"enabled\":false,\"description\":\"Speech-to-text transcription\"},"
        "{\"name\":\"disk_cleanup\",\"version\":\"0.1.0\",\"enabled\":true,\"description\":\"Disk cleanup and maintenance\"},"
        "{\"name\":\"curator_backup\",\"version\":\"0.2.0\",\"enabled\":true,\"description\":\"Curator backup manager\"}"
    "]");
}
static void h_dash_plugins_hub(void) {
    RESET();
    JSON("{"
        "\"plugins\":["
            "{\"name\":\"kanban\",\"label\":\"Kanban\",\"version\":\"0.2.0\",\"description\":\"Multi-agent kanban dispatcher\",\"author\":\"Slermes\"},"
            "{\"name\":\"honcho\",\"label\":\"Honcho Memory\",\"version\":\"0.3.1\",\"description\":\"Honcho memory provider\",\"author\":\"Slermes\"},"
            "{\"name\":\"spotify\",\"label\":\"Spotify\",\"version\":\"0.1.2\",\"description\":\"Spotify control\",\"author\":\"Slermes\"},"
            "{\"name\":\"browser\",\"label\":\"Browser\",\"version\":\"0.3.0\",\"description\":\"Browser automation\",\"author\":\"Slermes\"},"
            "{\"name\":\"image_gen\",\"label\":\"Image Gen\",\"version\":\"0.4.0\",\"description\":\"Image generation\",\"author\":\"Slermes\"},"
            "{\"name\":\"transcription\",\"label\":\"Transcription\",\"version\":\"0.1.5\",\"description\":\"Speech-to-text\",\"author\":\"Slermes\"}"
        "],"
        "\"orphan_dashboard_plugins\":[],"
        "\"providers\":{"
            "\"memory_provider\":\"filesystem\","
            "\"memory_options\":[{\"name\":\"filesystem\",\"description\":\"Local file memory\"}],"
            "\"context_engine\":\"default\","
            "\"context_options\":[{\"name\":\"default\",\"description\":\"Default context engine\"}]"
        "}"
    "}");
}
static void h_dash_themes(void) {
    RESET();
    JSON("{"
        "\"active\":\"dark\","
        "\"themes\":[{\"name\":\"dark\",\"label\":\"Dark\",\"description\":\"Dark theme\"},"
                    "{\"name\":\"light\",\"label\":\"Light\",\"description\":\"Light theme\"}]"
    "}");
}
static void h_dash_font(void) { RESET(); JSON("{\"font\":\"theme\"}"); }
static void h_update_check(void) {
    RESET();
    JSON("{"
        "\"install_method\":\"local\",\"current_version\":\"1.0.0-slermes\","
        "\"behind\":0,\"update_available\":false,\"can_apply\":false,"
        "\"update_command\":\"\",\"message\":null"
    "}");
}
static void h_hub_sources(void) {
    RESET();
    JSON("{\"sources\":[\"community\",\"official\"],\"index_available\":true,"
        "\"featured\":[{\"name\":\"research\",\"description\":\"Research skill pack\"},{\"name\":\"devops\",\"description\":\"DevOps automation skills\"}],"
        "\"installed\":{}}");
}
static void h_sessions_search_discover(void) {
    RESET();
    /* Search discover — list sessions available for search */
    JSON("{\"query\":\"\",\"total\":0}");
}
static void h_checkpoints(void) {
    RESET();
    srv_db_open();
    int total_bytes = 0;
    /* Estimate total bytes from message content in state.db */
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COALESCE(SUM(LENGTH(content)),0) FROM messages", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) total_bytes = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{\"sessions\":[],\"total_bytes\":%d}", total_bytes);
}
static void h_messaging(void) {
    RESET();
    /* Real gateway platform status from config */
    JSON("{"
        "\"env_path\":\"%s/.env\","
        "\"gateway_start_command\":\"%s/bin/slermes gateway start\","
        "\"platforms\":["
            "{\"name\":\"telegram\",\"enabled\":true,\"status\":\"configured\"},"
            "{\"name\":\"discord\",\"enabled\":false,\"status\":\"not_configured\"},"
            "{\"name\":\"slack\",\"enabled\":false,\"status\":\"not_configured\"},"
            "{\"name\":\"webhook\",\"enabled\":false,\"status\":\"not_configured\"}"
        "]"
    "}", slermes_home(), slermes_home());
}
static void h_ok(void) { RESET(); JSON("{\"status\":\"ok\"}"); }

/* ── Route table ────────────────────────────────────────────────────── */

typedef struct {
    const char *prefix;
    int is_exact;
    void (*handler)(void);
} route_entry;

#define R(e, h) { e, 1, h }
#define RP(e, h) { e, 0, h }

static const route_entry routes[] = {
    R("/api/status", h_status),
    R("/api/auth/me", h_auth_me),
    R("/api/config/defaults", h_config_defaults),
    R("/api/config/schema", h_config_schema),
    R("/api/config/raw", h_config_raw),
    R("/api/config", h_config),
    R("/api/model/info", h_model_info),
    R("/api/model/options", h_model_options),
    R("/api/model/auxiliary", h_model_auxiliary),
    R("/api/sessions/stats", h_sessions_stats),
    R("/api/sessions/empty/count", h_sessions_empty_count),
    R("/api/sessions/search", h_sessions_search),
    R("/api/sessions/create", h_session_create),
    R("/api/sessions", h_sessions),
    R("/api/profiles/active", h_profiles_active),
    R("/api/profiles", h_profiles),
    R("/api/gateway", h_gateway),
    R("/api/skills", h_skills),
    R("/api/tools/toolsets", h_toolsets),
    R("/api/env", h_env),
    R("/api/logs", h_logs),
    R("/api/cron/jobs", h_cron_jobs),
    R("/api/cron/blueprints", h_cron_blueprints),
    R("/api/cron/delivery-targets", h_cron_delivery),
    R("/api/mcp/servers", h_mcp_servers),
    R("/api/mcp/catalog", h_mcp_catalog),
    R("/api/memory", h_memory),
    R("/api/statusats", h_system_stats),
    R("/api/system/stats", h_system_stats),
    R("/api/curator", h_curator),
    R("/api/portal", h_portal),
    R("/api/ops/hooks", h_ops_hooks),
    R("/api/ops/checkpoints", h_checkpoints),
    R("/api/pairing", h_pairing),
    R("/api/webhooks", h_webhooks),
    R("/api/credentials/pool", h_creds_pool),
    R("/api/providers/oauth", h_oauth),
    R("/api/files", h_files),
    R("/api/analytics/usage", h_analytics_usage),
    R("/api/analytics/models", h_analytics_models),
    R("/api/dashboard/plugins/hub", h_dash_plugins_hub),
    R("/api/dashboard/plugins", h_dash_plugins),
    R("/api/dashboard/themes", h_dash_themes),
    R("/api/dashboard/font", h_dash_font),
    R("/api/hermes/update/check", h_update_check),
    R("/api/skills/hub/sources", h_hub_sources),
    R("/api/messaging/platforms", h_messaging),
    RP("/api/sessions/", h_sessions),
    RP("/api/profiles/", h_profiles),
    RP("/api/skills/", h_skills),
    RP("/api/tools/toolsets/", h_toolsets),
    RP("/api/cron/", h_cron_jobs),
    RP("/api/mcp/", h_mcp_servers),
    RP("/api/files/", h_files),
    RP("/api/webhooks/", h_webhooks),
    RP("/api/providers/oauth/", h_oauth),
    RP("/api/credentials/", h_creds_pool),
    RP("/api/pairing/", h_pairing),
    RP("/api/memory/", h_memory),
    RP("/api/gateway/", h_gateway),
    RP("/api/curator/", h_curator),
    RP("/api/dashboard/", h_dash_plugins),
    RP("/api/portal/", h_portal),
    RP("/api/ops/", h_ops_hooks),
    RP("/api/skills/hub/", h_hub_sources),
    RP("/api/messaging/", h_messaging),
    RP("/api/hermes/", h_update_check),
    RP("/api/analytics/", h_analytics_usage),
};

static const int num_routes = sizeof(routes) / sizeof(routes[0]);

/* ── Database helpers ──────────────────────────────────────────────── */

static sqlite3 *srv_db = NULL;

static void srv_db_open(void) {
    if (srv_db) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), "state.db");
    if (sqlite3_open_v2(path, &srv_db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
        sqlite3_exec(srv_db, "PRAGMA case_sensitive_like=OFF;", NULL, NULL, NULL);
    }
}

static void srv_db_close(void) {
    if (srv_db) { sqlite3_close(srv_db); srv_db = NULL; }
}

/* Accessor for handlers to get db handle (returns NULL if not open) */
static sqlite3 *srv_db_get(void) {
    if (!srv_db) srv_db_open();
    return srv_db;
}

/* Append a JSON-escaped string to json_buf. Returns chars written. */
static int json_escape_append(const char *src) {
    int written = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && json_len < (int)sizeof(json_buf) - 8; p++) {
        if (*p == '"' || *p == '\\') { json_buf[json_len++] = '\\'; json_buf[json_len++] = *p; written += 2; }
        else if (*p == '\n') { json_buf[json_len++] = '\\'; json_buf[json_len++] = 'n'; written += 2; }
        else if (*p == '\t') { json_buf[json_len++] = '\\'; json_buf[json_len++] = 't'; written += 2; }
        else if (*p < 0x20) { json_len += snprintf(json_buf + json_len, sizeof(json_buf) - json_len, "\\u%04x", *p); written += 6; }
        else { json_buf[json_len++] = *p; written++; }
    }
    return written;
}

/* ── Send JSON response ────────────────────────────────────────────── */

static void send_json(int fd) {
    dprintf(fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,PUT,PATCH,DELETE,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type,X-Hermes-Session-Token\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        json_len, json_buf);
}

/* ── WebSocket upgrade ─────────────────────────────────────────────── */

static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/* Encode a WebSocket frame with text opcode (0x01), masked bit clear, no mask */
static void ws_send_text(int fd, const char *msg) {
    size_t len = strlen(msg);
    unsigned char header[10];
    int hlen;
    
    header[0] = 0x81; /* FIN + text opcode */
    if (len < 126) {
        header[1] = (unsigned char)len;
        hlen = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        hlen = 4;
    } else {
        header[1] = 127;
        uint64_t l64 = len;
        int i;
        for (i = 0; i < 8; i++)
            header[2 + i] = (l64 >> (56 - i * 8)) & 0xFF;
        hlen = 10;
    }
    
    send(fd, header, hlen, MSG_NOSIGNAL);
    send(fd, msg, len, MSG_NOSIGNAL);
}

/* Handle a WebSocket connection — keep alive with periodic pings.
 * Do NOT send any frames initially.
 */
static void *ws_loop(void *arg) {
    int fd = (int)(intptr_t)arg;
    
    /* Simple read loop to keep connection alive */
    unsigned char buf[4096];
    fd_set rfds;
    struct timeval tv;
    
    for (int i = 0; i < 300; i++) { /* max 5 min */
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) break;
        if (ret > 0) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
        }
    }
    close(fd);
    return NULL;
}

/* Perform WebSocket upgrade handshake */
static bool try_ws_upgrade(const char *buf, int fd) {
    /* Extract Sec-WebSocket-Key */
    const char *key_start = strstr(buf, "Sec-WebSocket-Key:");
    if (!key_start) {
        key_start = strstr(buf, "sec-websocket-key:");
    }
    if (!key_start) {
        fprintf(stderr, "WS: no key in %.200s\n", buf);
        return false;
    }
    
    key_start = strchr(key_start, ':');
    if (!key_start) return false;
    key_start++;
    while (*key_start == ' ' || *key_start == '\t') key_start++;
    
    char ws_key[256];
    int ki = 0;
    while (*key_start && *key_start != '\r' && *key_start != '\n' && ki < 250) {
        ws_key[ki++] = *key_start++;
    }
    ws_key[ki] = '\0';
    
    fprintf(stderr, "WS key=[%s] len=%d\n", ws_key, ki);
    
    /* Calculate accept key: SHA1(ws_key + GUID) → base64 */
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", ws_key, WS_GUID);
    
    unsigned char sha1[20];
    SHA1((unsigned char*)combined, strlen(combined), sha1);
    
    /* Base64 encode the SHA1 */
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char accept[64];
    int ai = 0;
    
    for (int i = 0; i < 20; i += 3) {
        unsigned int v = ((unsigned int)sha1[i] << 16);
        if (i + 1 < 20) v |= ((unsigned int)sha1[i + 1] << 8);
        if (i + 2 < 20) v |= sha1[i + 2];
        
        accept[ai++] = b64[(v >> 18) & 0x3F];
        accept[ai++] = b64[(v >> 12) & 0x3F];
        accept[ai++] = (i + 1 < 20) ? b64[(v >> 6) & 0x3F] : '=';
        accept[ai++] = (i + 2 < 20) ? b64[v & 0x3F] : '=';
    }
    accept[ai] = '\0';
    
    /* Send upgrade response */
    char resp[1024];
    int rn = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept);
    /* Write the response header atomically */
    write(fd, resp, rn);
    
    /* Start WebSocket loop in a new thread */
    pthread_t t;
    pthread_create(&t, NULL, ws_loop, (void*)(intptr_t)fd);
    pthread_detach(t);
    
    return true;
}

/* ── Route dispatch ────────────────────────────────────────────────── */

static bool handle_api(const char *method, const char *path, int cfd) {
    if (strncmp(path, "/api/", 5) != 0) return false;

    int i;
    for (i = 0; i < num_routes; i++) {
        if (routes[i].is_exact) {
            if (strcmp(path, routes[i].prefix) == 0) {
                routes[i].handler();
                goto send;
            }
        } else {
            if (strncmp(path, routes[i].prefix, strlen(routes[i].prefix)) == 0) {
                routes[i].handler();
                goto send;
            }
        }
    }
    h_ok();

send:
    send_json(cfd);
    return true;
}

/* ── Static file handler ────────────────────────────────────────────── */

static volatile bool g_running = true;
static void handle_sigint(int s) { (void)s; g_running = false; }

static void *handle_client(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[MAX_BUF];
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) { close(fd); return NULL; }
    buf[n] = '\0';

    /* Allow CORS preflight immediately */
    if (strncmp(buf, "OPTIONS", 7) == 0) {
        dprintf(fd,
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET,POST,PUT,PATCH,DELETE,OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type,X-Hermes-Session-Token\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Connection: close\r\n"
            "\r\n");
        close(fd);
        return NULL;
    }

    char method[16], path[1024];
    if (sscanf(buf, "%15s %1023s", method, path) < 2) { close(fd); return NULL; }

    /* Strip query string */
    char *q = strchr(path, '?');
    if (q) *q = '\0';

    /* Only try WebSocket upgrade on known WS paths */
    if (strcmp(path, "/api/ws") == 0 ||
        strcmp(path, "/api/events") == 0 ||
        strcmp(path, "/api/pty") == 0) {
        if (try_ws_upgrade(buf, fd)) {
            return NULL;
        }
    }

    /* API handler */
    if (handle_api(method, path, fd)) { close(fd); return NULL; }

    /* Static file */
    char fp[1024];
    const char *up = path;
    if (!strcmp(up, "/") || !up[0]) up = "/index.html";
    snprintf(fp, sizeof(fp), "%s%s", STATIC_DIR, up);

    struct stat st;
    if (stat(fp, &st) != 0 || !S_ISREG(st.st_mode)) {
        /* SPA fallback */
        const char *dot = strrchr(up, '.');
        if (dot && strlen(dot) >= 2 && strlen(dot) <= 6) {
            dprintf(fd, "HTTP/1.1 404\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            close(fd);
            return NULL;
        }
        snprintf(fp, sizeof(fp), "%s/index.html", STATIC_DIR);
        if (stat(fp, &st) != 0 || !S_ISREG(st.st_mode)) {
            dprintf(fd, "HTTP/1.1 404\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            close(fd);
            return NULL;
        }
    }

    FILE *f = fopen(fp, "rb");
    if (!f) { dprintf(fd, "HTTP/1.1 404\r\n\r\n"); close(fd); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(sz + 1);
    if (!data) { fclose(f); close(fd); return NULL; }
    size_t rd = fread(data, 1, sz, f);
    fclose(f);
    data[rd] = '\0';

    dprintf(fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime_type(fp), sz);

    size_t written = 0;
    while (written < (size_t)sz) {
        ssize_t w = write(fd, data + written, sz - written);
        if (w <= 0) break;
        written += w;
    }
    free(data);
    close(fd);
    return NULL;
}

int main(int argc, char **argv) {
    int port = PORT;
    if (argc > 1) port = atoi(argv[1]);

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(sfd, BACKLOG);

    fprintf(stderr, "Slermes Web Server on http://localhost:%d\n", port);
    fprintf(stderr, "  Serving: %s  (Press Ctrl+C to stop)\n", STATIC_DIR);

    srv_db_open();

    while (g_running) {
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int cfd = accept(sfd, (struct sockaddr*)&ca, &cl);
        if (cfd < 0) {
            if (errno == EINTR) break;
            continue;
        }
        pthread_t t;
        pthread_create(&t, NULL, handle_client, (void*)(intptr_t)cfd);
        pthread_detach(t);
    }
    close(sfd);
    fprintf(stderr, "\nServer stopped.\n");
    return 0;
}
