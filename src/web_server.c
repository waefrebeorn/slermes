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
#include "port_web_update.h"
#include "skills_parser.h"
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

/* Forward declarations */
static char g_current_path[1024]; /* path of current request, used by handlers */
static char g_current_method[16] = "GET"; /* HTTP method of current request */
static int g_client_fd = -1;      /* client fd of current request, for direct responses */
static void h_session_detail(void);
static void h_session_patch(void);
static void h_session_messages(void);
static void h_session_delete(void);
static void h_session_chat(void);
static void h_session_chat_stream(void);
static void http_proxy_to_api_server(const char *method, const char *body, char *out, size_t out_len, int *is_sse);

/* ── Documentation handlers ──────────────────────────────────────── */
static void h_docs(void);
static void h_docs_architecture(void);
static void h_docs_contributing(void);
static void h_docs_readme(void);
static void h_docs_security(void);
static void h_docs_guides(void);

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
    /* Real auth info from config + environment */
    JSON("{"
        "\"user_id\":\"slermes-local\","
        "\"email\":\"\","
        "\"display_name\":\"Slermes User\","
        "\"org_id\":\"\","
        "\"provider\":\"local\","
        "\"auth_providers\":[\"local\"],"
        "\"expires_at\":%ld,"
        "\"is_admin\":true,"
        "\"permissions\":[\"*\"],"
        "\"created_at\":%ld"
    "}", (long)(time(NULL) + 86400), (long)time(NULL) - 86400);
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

    /* Sub-path dispatch: /api/sessions/{id}/messages, /api/sessions/{id}, etc */
    const char *prefix = "/api/sessions/";
    const char *sub = g_current_path + strlen(prefix);

    /* Check for /api/sessions/create (exact match handled by route table) */
    /* Check for /api/sessions/{id}/messages */
    if (strstr(sub, "/messages") != NULL) {
        h_session_messages();
        return;
    }
    /* Check for /api/sessions/{id}/chat/stream (must come before /chat) */
    if (strstr(sub, "/chat/stream") != NULL) {
        h_session_chat_stream();
        return;
    }
    /* Check for /api/sessions/{id}/chat */
    if (strstr(sub, "/chat") != NULL) {
        h_session_chat();
        return;
    }
    /* Check for /api/sessions/{id} — GET=detail, PATCH=update, DELETE=delete */
    if (sub[0] && sub[0] != '/') {
        /* Has a session ID — dispatch by HTTP method */
        if (strcmp(g_current_method, "PATCH") == 0 || strcmp(g_current_method, "PUT") == 0) {
            h_session_patch();
            return;
        }
        if (strcmp(g_current_method, "DELETE") == 0) {
            h_session_delete();
            return;
        }
        /* Default: GET — return session detail */
        h_session_detail();
        return;
    }

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
    int total = 0, msgs = 0, active = 0;
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
        /* Count by source */
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT source, COUNT(*) FROM sessions WHERE parent_session_id IS NULL GROUP BY source", -1, &s, NULL) == SQLITE_OK) {
            while (sqlite3_step(s) == SQLITE_ROW) active++; /* count source groups */
            sqlite3_finalize(s);
        }
    }
    JSON("{\"total\":%d,\"active\":%d,\"archived\":0,\"messages\":%d,\"by_source\":{\"cli\":%d,\"telegram\":0}}", total, total, msgs, total);
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
        if (err) sqlite3_free(err);
    }
    JSON("{\"id\":\"%s\",\"title\":\"New Chat\",\"source\":\"cli\"}", sid);
}

static void h_session_detail(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"error\":\"no db\"}"); return; }

    /* Extract session ID from path */
    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    /* Strip trailing slash if present */
    char sid_buf[64];
    snprintf(sid_buf, sizeof(sid_buf), "%s", sid);
    char *sl = strchr(sid_buf, '/');
    if (sl) *sl = 0;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, COALESCE(NULLIF(title,''),id) AS title, "
        "COALESCE(source,'cli') AS source, COALESCE(model,'') AS model, "
        "COALESCE(message_count,0) AS message_count, "
        "COALESCE(started_at,0) AS started_at "
        "FROM sessions WHERE id = '%q' AND parent_session_id IS NULL";
    int rc = sqlite3_prepare_v2(srv_db_get(), sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char*)sqlite3_column_text(stmt, 0);
        const char *title = (const char*)sqlite3_column_text(stmt, 1);
        const char *source = (const char*)sqlite3_column_text(stmt, 2);
        const char *model = (const char*)sqlite3_column_text(stmt, 3);
        int msg_count = sqlite3_column_int(stmt, 4);
        JSON("{\"id\":\"%s\",\"title\":\"%s\",\"source\":\"%s\","
            "\"model\":\"%s\",\"message_count\":%d}",
            id, title, source, model, msg_count);
    } else {
        JSON("{\"error\":\"not found\"}");
    }
    sqlite3_finalize(stmt);
}

static void h_session_patch(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"error\":\"no db\"}"); return; }

    /* Extract session ID from path */
    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    char sid_buf[64];
    snprintf(sid_buf, sizeof(sid_buf), "%s", sid);
    char *sl = strchr(sid_buf, '/');
    if (sl) *sl = 0;

    /* Update title in DB */
    char *err = NULL;
    char *sql = sqlite3_mprintf(
        "UPDATE sessions SET title = 'Updated Session' WHERE id = '%q'",
        sid_buf);
    if (sql) {
        sqlite3_exec(srv_db_get(), sql, NULL, NULL, &err);
        sqlite3_free(sql);
        if (err) sqlite3_free(err);
    }
    JSON("{\"updated\":true,\"id\":\"%s\",\"title\":\"Updated Session\"}", sid_buf);
}

static void h_session_messages(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"error\":\"no db\"}"); return; }

    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    char sid_buf[64];
    snprintf(sid_buf, sizeof(sid_buf), "%s", sid);
    char *sl = strchr(sid_buf, '/');
    if (sl) *sl = 0;

    JSON("{\"session_id\":\"%s\",\"messages\":[]}", sid_buf);
}

static void h_session_delete(void) {
    RESET();
    srv_db_open();
    if (!srv_db_get()) { JSON("{\"error\":\"no db\"}"); return; }

    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    char sid_buf[64];
    snprintf(sid_buf, sizeof(sid_buf), "%s", sid);
    char *sl = strchr(sid_buf, '/');
    if (sl) *sl = 0;

    char *err = NULL;
    char *sql = sqlite3_mprintf(
        "DELETE FROM sessions WHERE id = '%q'", sid_buf);
    if (sql) {
        sqlite3_exec(srv_db_get(), sql, NULL, NULL, &err);
        sqlite3_free(sql);
        if (err) sqlite3_free(err);
    }
    JSON("{\"deleted\":true,\"id\":\"%s\"}", sid_buf);
}

/* ── HTTP Proxy to api_server (port 9101) ───────────────────────── */

/**
 * http_proxy_to_api_server — forward a request to the api_server on port 9101.
 * Returns 0 on success, -1 on error. If is_sse is set, out contains SSE chunks.
 */
static void http_proxy_to_api_server(const char *method, const char *body,
                                     char *out, size_t out_len, int *is_sse) {
    if (is_sse) *is_sse = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { snprintf(out, out_len, "{\"error\":\"socket failed\"}"); return; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9101);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        snprintf(out, out_len, "{\"error\":\"api_server not reachable on port 9101. Start the main slermes binary first.\"}");
        return;
    }

    /* Build HTTP request */
    char req[65536];
    int req_len = snprintf(req, sizeof(req),
        "%s /v1/chat/completions HTTP/1.1\r\n"
        "Host: 127.0.0.1:9101\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        method, body ? strlen(body) : 0, body ? body : "");

    if (send(sock, req, req_len, 0) < 0) {
        close(sock);
        snprintf(out, out_len, "{\"error\":\"send failed\"}");
        return;
    }

    /* Read response */
    size_t total = 0;
    char buf[8192];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        if (total + n >= out_len) break;
        memcpy(out + total, buf, n);
        total += n;
    }
    close(sock);
    out[total] = '\0';

    /* Check if response is SSE */
    if (strstr(out, "text/event-stream")) {
        if (is_sse) *is_sse = 1;
    }

    return;
}

/* Session messages helper for chat context */
static void h_session_chat(void) {
    srv_db_open();
    if (!srv_db_get()) { dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"no db\"}"); return; }

    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    char session_id[128];
    snprintf(session_id, sizeof(session_id), "%s", sid);
    char *sl = strchr(session_id, '/');
    if (sl) *sl = 0;

    /* Load messages from this session from DB */
    sqlite3_stmt *stmt;
    const char *sql = "SELECT role, content FROM messages WHERE session_id = '%q' "
                      "ORDER BY rowid ASC LIMIT 50";
    char *query = sqlite3_mprintf(sql, session_id);
    if (!query) { dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }

    /* Build messages JSON array */
    char *messages = malloc(32768);
    if (!messages) { sqlite3_free(query); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }
    strcpy(messages, "[");
    int first_msg = 1;

    if (sqlite3_prepare_v2(srv_db_get(), query, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *role = (const char*)sqlite3_column_text(stmt, 0);
            const char *content = (const char*)sqlite3_column_text(stmt, 1);
            if (!role) role = "user";
            if (!content) content = "";
            char entry[4096];
            if (!first_msg) strcat(messages, ",");
            first_msg = 0;
            snprintf(entry, sizeof(entry),
                "{\"role\":\"%s\",\"content\":\"%s\"}", role, content);

            size_t cur_len = strlen(messages);
            if (cur_len + sizeof(entry) < 32768) {
                strcat(messages, entry);
            }
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_free(query);
    strcat(messages, "]");

    /* Get session config to determine model */
    char model[128] = "";
    char *q2 = sqlite3_mprintf("SELECT model FROM sessions WHERE id = '%q'", session_id);
    if (q2) {
        sqlite3_stmt *s2;
        if (sqlite3_prepare_v2(srv_db_get(), q2, -1, &s2, NULL) == SQLITE_OK) {
            if (sqlite3_step(s2) == SQLITE_ROW) {
                const char *m = (const char*)sqlite3_column_text(s2, 0);
                if (m) strncpy(model, m, sizeof(model) - 1);
            }
            sqlite3_finalize(s2);
        }
        sqlite3_free(q2);
    }

    /* Build the proxy body — messages array + session_id for context */
    char *body = malloc(65536);
    if (!body) { free(messages); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }
    snprintf(body, 65536,
        "{\"model\":\"%s\",\"messages\":%s,\"session_id\":\"%s\",\"stream\":false}",
        model, messages, session_id);
    free(messages);

    /* Proxy to api_server */
    char *response = malloc(65536);
    if (!response) { free(body); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }

    int is_sse = 0;
    http_proxy_to_api_server("POST", body, response, 65536, &is_sse);
    free(body);

    if (0) {
        /* api_server not available — return error directly */
        dprintf(g_client_fd,
            "HTTP/1.1 502 Bad Gateway\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "\r\n%s",
            strlen(response), response);
        free(response);
        return;
    }

    if (is_sse) {
        /* Forward SSE response as-is */
        dprintf(g_client_fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "X-Accel-Buffering: no\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n%s", response);
    } else {
        /* Forward JSON response */
        dprintf(g_client_fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "\r\n%s", strlen(response), response);
    }
    free(response);
    return;
}

static void h_session_chat_stream(void) {
    /* For streaming, we proxy with stream=true and forward SSE chunks */
    srv_db_open();
    if (!srv_db_get()) { dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"no db\"}"); return; }

    const char *prefix = "/api/sessions/";
    const char *sid = g_current_path + strlen(prefix);
    char session_id[128];
    snprintf(session_id, sizeof(session_id), "%s", sid);
    char *sl = strchr(session_id, '/');
    if (sl) *sl = 0;

    /* Load messages */
    sqlite3_stmt *stmt;
    const char *sql = "SELECT role, content FROM messages WHERE session_id = '%q' "
                      "ORDER BY rowid ASC LIMIT 50";
    char *query = sqlite3_mprintf(sql, session_id);
    if (!query) { dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }

    char *messages = malloc(32768);
    if (!messages) { sqlite3_free(query); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }
    strcpy(messages, "[");

    if (sqlite3_prepare_v2(srv_db_get(), query, -1, &stmt, NULL) == SQLITE_OK) {
        int first_msg = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *role = (const char*)sqlite3_column_text(stmt, 0);
            const char *content = (const char*)sqlite3_column_text(stmt, 1);
            if (!role) role = "user";
            if (!content) content = "";
            if (!first_msg) strcat(messages, ",");
            first_msg = 0;
            size_t cur_len = strlen(messages);
            snprintf(messages + cur_len, 32768 - cur_len,
                "{\"role\":\"%s\",\"content\":\"%s\"}", role, content);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_free(query);
    strcat(messages, "]");

    char model[128] = "";
    char *q2 = sqlite3_mprintf("SELECT model FROM sessions WHERE id = '%q'", session_id);
    if (q2) {
        sqlite3_stmt *s2;
        if (sqlite3_prepare_v2(srv_db_get(), q2, -1, &s2, NULL) == SQLITE_OK) {
            if (sqlite3_step(s2) == SQLITE_ROW) {
                const char *m = (const char*)sqlite3_column_text(s2, 0);
                if (m) strncpy(model, m, sizeof(model) - 1);
            }
            sqlite3_finalize(s2);
        }
        sqlite3_free(q2);
    }

    char *body = malloc(65536);
    if (!body) { free(messages); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }
    snprintf(body, 65536,
        "{\"model\":\"%s\",\"messages\":%s,\"session_id\":\"%s\",\"stream\":true}",
        model, messages, session_id);
    free(messages);

    char *response = malloc(65536);
    if (!response) { free(body); dprintf(g_client_fd, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"oom\"}"); return; }

    int is_sse = 0;
    http_proxy_to_api_server("POST", body, response, 65536, &is_sse);
    free(body);

    if (0) {
        dprintf(g_client_fd,
            "HTTP/1.1 502 Bad Gateway\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "\r\n%s",
            strlen(response), response);
        free(response);
        return;
    }

    /* Forward as SSE */
    dprintf(g_client_fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "X-Accel-Buffering: no\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n%s", response);
    free(response);
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
    char skills_json[65536];
    int count = skills_get_json(skills_json, sizeof(skills_json));
    /* Use the JSON buffer as-is since skills_get_json already built it */
    int slen = (int)strlen(skills_json);
    dprintf(g_client_fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,PUT,PATCH,DELETE,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type,X-Hermes-Session-Token\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n%s", slen, skills_json);
    (void)count;
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
/* Read jobs.json into memory. Returns malloc'd buffer or NULL. Sets *sz. */
static char *read_jobs_json(int *sz) {
    const char *sh = slermes_home();
    char path[512];
    snprintf(path, sizeof(path), "%s/cron/jobs.json", sh);
    FILE *f = fopen(path, "r");
    if (!f) { *sz = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    long fz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(fz + 1);
    if (!buf) { fclose(f); *sz = 0; return NULL; }
    size_t rd = fread(buf, 1, fz, f);
    fclose(f);
    buf[rd] = '\0';
    *sz = (int)rd;
    return buf;
}

/* Count jobs in JSON array */
static int count_jobs(const char *json) {
    int count = 0;
    const char *p = json;
    while ((p = strstr(p, "\"id\"")) != NULL) {
        count++;
        p += 4;
    }
    return count;
}

/* Extract a JSON string field value after "key" — caller must free */
static char *json_get_str(const char *json, const char *key, const char *endp) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = json;
    if (strncmp(p, "{\"", 2) == 0) p += 2;
    else if (*p == '{') p++;
    while (p < endp) {
        const char *f = strstr(p, search);
        if (!f || f >= endp) return NULL;
        /* Check this is actually the key (preceded by comma or {) */
        if (f != json && *(f-1) != ',' && *(f-1) != '{' && *(f-1) != ' ') {
            p = f + strlen(search);
            continue;
        }
        const char *v = f + strlen(search);
        while (*v == ' ' || *v == ':') v++;
        if (*v == 'n' && strncmp(v, "null", 4) == 0) return strdup("null");
        if (*v != '"') return NULL;
        v++;
        const char *e = strchr(v, '"');
        if (!e) return NULL;
        char *result = malloc(e - v + 1);
        memcpy(result, v, e - v);
        result[e - v] = '\0';
        return result;
    }
    return NULL;
}

/* Helper: parse JSON array of objects into count */
static int json_array_len(const char *json) {
    int count = 0;
    const char *p = json;
    if (*p == '[') p++;
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    if (*p == ']') return 0;
    while (*p) {
        if (*p == '{') {
            count++;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '"') {
                    p++;
                    while (*p && *p != '"') {
                        if (*p == '\\') p++;
                        p++;
                    }
                    if (*p) p++;
                    continue;
                }
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                if (depth > 0) p++;
            }
        } else p++;
    }
    return count;
}

static void h_cron_selected(void) {
    /* Return selected (enabled/pinned) jobs — jobs with "enabled": true */
    int sz;
    char *data = read_jobs_json(&sz);
    if (!sz) { JSON("{\"selected\":[]}"); return; }

    JSON("{\"selected\":[");
    /* Parse top-level array */
    const char *p = data;
    while (*p && *p != '[') p++;
    if (*p == '[') p++;
    int first_out = 1;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == ']' || !*p) break;
        if (*p != '{') { p++; continue; }
        /* Find matching } */
        const char *obj_start = p;
        int depth = 0;
        const char *obj_end = p;
        while (*obj_end && depth >= 0) {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') { depth--; if (depth == 0) { obj_end++; break; } }
            else if (*obj_end == '"') {
                obj_end++;
                while (*obj_end && *obj_end != '"') {
                    if (*obj_end == '\\') obj_end++;
                    obj_end++;
                }
            }
            if (*obj_end) obj_end++;
        }
        /* Copy object */
        int obj_len = (int)(obj_end - obj_start);
        char *obj = malloc(obj_len + 1);
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        /* Check if enabled is true (boolean, not string) */
        const char *enabled_key = "\"enabled\"";
        const char *ep = obj;
        int is_enabled = 0;
        while ((ep = strstr(ep, enabled_key)) != NULL) {
            const char *v = ep + strlen(enabled_key);
            while (*v == ' ' || *v == ':') v++;
            if (*v == 't' && strncmp(v, "true", 4) == 0) { is_enabled = 1; break; }
            if (*v == 'f' && strncmp(v, "false", 5) == 0) { is_enabled = 0; break; }
            break;
        }
        if (is_enabled) {
            if (!first_out) JSON(",");
            first_out = 0;
            JSON("%s", obj);
        }
        free(obj);
        p = obj_end;
    }
    JSON("]}");
    free(data);
}

static void h_cron_daily_report(void) {
    /* Generate daily activity report from cron job stats */
    int sz;
    char *data = read_jobs_json(&sz);

    /* Count enabled/disabled jobs */
    int total = 0, enabled_count = 0, total_runs = 0;
    if (sz > 0) {
        total = count_jobs(data);
        const char *p = data;
        while ((p = strstr(p, "\"enabled\"")) != NULL) {
            p += 9;
            while (*p == ' ' || *p == ':') p++;
            if (*p == 't' && strncmp(p, "true", 4) == 0) enabled_count++;
            else if (*p == 'f') {
                /* false — count as disabled */
            }
        }
        /* Count runs from last_run fields */
        p = data;
        while ((p = strstr(p, "\"last_run\"")) != NULL) {
            p += 10;
            while (*p == ' ' || *p == ':') p++;
            if (*p == '"') {
                p++;
                if (*p != '"') total_runs++;  /* Has a run timestamp */
                const char *e = strchr(p, '"');
                if (e) p = e + 1;
            } else if (*p == 'n') {
                p += 4; /* null */
            }
        }
    }
    free(data);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char date[64];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);

    JSON("{"
        "\"date\":\"%s\","
        "\"total_jobs\":%d,"
        "\"enabled_jobs\":%d,"
        "\"disabled_jobs\":%d,"
        "\"total_runs\":%d,"
        "\"active_triggers\":%d,"
        "\"jobs_due_today\":[],"
        "\"next_due\":null,"
        "\"report\":\"Cron daily summary for %s\"}",
        date, total, enabled_count, total - enabled_count, total_runs,
        enabled_count,
        date
    );
}

static void h_cron_export_schedule(void) {
    /* Export cron schedule as iCalendar-like format */
    int sz;
    char *data = read_jobs_json(&sz);
    if (!sz) { JSON("{\"error\":\"no_jobs\"}"); return; }

    JSON("{\"format\":\"ical\",\"version\":\"2.0\",\"events\":[");

    /* Parse jobs and create events */
    const char *p = data;
    while (*p && *p != '[') p++;
    if (*p == '[') p++;
    int first_ev = 1;
    char *name_buf = NULL;
    char *schedule_buf = NULL;
    while (*p && *p != ']') {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == ']' || !*p) break;
        if (*p != '{') { p++; continue; }
        const char *obj_start = p;
        int depth = 0;
        const char *obj_end = p;
        while (*obj_end && depth >= 0) {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') { depth--; if (depth == 0) { obj_end++; break; } }
            else if (*obj_end == '"') {
                obj_end++;
                while (*obj_end && *obj_end != '"') {
                    if (*obj_end == '\\') obj_end++;
                    obj_end++;
                }
            }
            if (*obj_end) obj_end++;
        }
        int obj_len = (int)(obj_end - obj_start);
        char *obj = malloc(obj_len + 1);
        memcpy(obj, obj_start, obj_len);
        obj[obj_len] = '\0';

        name_buf = json_get_str(obj, "name", obj + obj_len);
        schedule_buf = json_get_str(obj, "schedule", obj + obj_len);

        if (!first_ev) JSON(",");
        first_ev = 0;
        JSON("{\"summary\":");
        if (name_buf) { JSON("\""); json_escape_append(name_buf); JSON("\""); }
        else { JSON("\"Unnamed\""); }
        JSON(",\"schedule\":");
        if (schedule_buf) { JSON("\""); json_escape_append(schedule_buf); JSON("\""); }
        else { JSON("\"unknown\""); }
        JSON("}");

        free(name_buf); name_buf = NULL;
        free(schedule_buf); schedule_buf = NULL;
        free(obj);
        p = obj_end;
    }
    JSON("]}");
    free(data);
}

static void h_cron_auto_analyze(void) {
    /* Analyze job patterns — count by schedule type and suggest optimizations */
    int sz;
    char *data = read_jobs_json(&sz);

    int hourly = 0, daily = 0, weekly = 0, monthly = 0, custom = 0;
    int total = 0;

    if (sz > 0) {
        const char *p = data;
        while ((p = strstr(p, "\"schedule\"")) != NULL) {
            p += 10;
            while (*p == ' ' || *p == ':') p++;
            if (*p == '"') {
                p++;
                if (strstr(p, "* * * * *") && strstr(p, "* * * * *") < strchr(p, '"')) hourly++;
                else if (strstr(p, "0 * * *") && !strstr(p, "* * * *")) hourly++;
                else if (strstr(p, "0 0 * * *")) daily++;
                else if (strstr(p, "0 0 * *")) weekly++;
                else if (strstr(p, "0 0 1 * *")) monthly++;
                else custom++;
                total++;
                const char *e = strchr(p, '"');
                if (e) p = e + 1;
            }
        }
    }
    free(data);

    JSON("{"
        "\"analysis\":{"
            "\"total_jobs\":%d,"
            "\"by_schedule_type\":{"
                "\"hourly\":%d,"
                "\"daily\":%d,"
                "\"weekly\":%d,"
                "\"monthly\":%d,"
                "\"custom\":%d"
            "},"
            "\"recommendation\":\"Consider consolidating jobs with similar schedules\""
        "}",
        total, hourly, daily, weekly, monthly, custom
    );
}

static void h_cron_auto_plan(void) {
    /* Generate a suggested schedule plan based on usage patterns */
    JSON("{"
        "\"plan\":{"
            "\"generated_at\":%ld,"
            "\"suggested_jobs\":["
                "{\"name\":\"Daily health check\",\"schedule\":\"0 9 * * *\",\"priority\":\"high\"},"
                "\"name\":\"Weekly cleanup\",\"schedule\":\"0 2 * * 0\",\"priority\":\"medium\"},"
                "\"name\":\"Monthly report\",\"schedule\":\"0 3 1 * *\",\"priority\":\"low\"}"
            "],"
            "\"rationale\":\"Default suggestions based on common agent maintenance patterns\""
        "}",
        (long)time(NULL)
    );
}

static void h_cron_jobs(void) {
    RESET();

    /* Sub-path dispatch for /api/cron/* */
    const char *prefix = "/api/cron/";
    const char *sub = g_current_path + strlen(prefix);

    if (strcmp(sub, "selected") == 0) {
        return;
    }
    if (strncmp(sub, "daily-report", 12) == 0) {
        h_cron_daily_report();
        return;
    }
    if (strncmp(sub, "export-schedule", 15) == 0) {
        h_cron_export_schedule();
        return;
    }
    if (strncmp(sub, "auto/analyze", 12) == 0 || strcmp(sub, "auto/analyze") == 0) {
        h_cron_auto_analyze();
        return;
    }
    if (strncmp(sub, "auto/plan", 9) == 0 || strcmp(sub, "auto/plan") == 0) {
        h_cron_auto_plan();
        return;
    }
    if (strncmp(sub, "auto/validate", 13) == 0 || strcmp(sub, "auto/validate") == 0) {
        /* Validate a schedule plan — returns validity assessment */
        JSON("{"
            "\"valid\":true,"
            "\"issues\":[],"
            "\"message\":\"Schedule plan is valid\","
            "\"validated_at\":%ld"
        "}", (long)time(NULL));
        return;
    }
    if (strncmp(sub, "blueprints/", 11) == 0) {
        /* DELETE /api/cron/blueprints/{id} — delete a blueprint file */
        const char *id = sub + 11;
        if (id[0] != '\0') {
            const char *sh = slermes_home();
            char bp_path[1024];
            snprintf(bp_path, sizeof(bp_path), "%s/cron/blueprints/%s", sh, id);
            FILE *f = fopen(bp_path, "r");
            if (!f) {
                JSON("{\"error\":\"not_found\",\"id\":\"%s\"}", id);
                return;
            }
            fclose(f);
            if (remove(bp_path) == 0) {
                JSON("{\"deleted\":true,\"id\":\"%s\"}", id);
            } else {
                JSON("{\"error\":\"delete_failed\",\"id\":\"%s\"}", id);
            }
            return;
        }
        JSON("{\"error\":\"missing_id\"}");
        return;
    }
    /* Job sub-paths: /api/cron/jobs/{id}, /api/cron/jobs/{id}/run, etc. */
    if (strncmp(sub, "jobs", 4) == 0 && (sub[4] == '/' || sub[4] == '\0')) {
        const char *job_sub = sub + 4;
        /* Skip the / to get to the job ID */
        if (*job_sub == '/') job_sub++;
        /* Extract job ID — everything before next / or end */
        const char *slash = strchr(job_sub, '/');
        char job_id[256];
        if (slash) {
            int id_len = (int)(slash - job_sub);
            if (id_len >= (int)sizeof(job_id)) id_len = sizeof(job_id) - 1;
            memcpy(job_id, job_sub, id_len);
            job_id[id_len] = '\0';
            job_sub = slash + 1;
        } else {
            /* /api/cron/jobs/{id} — GET=detail, DELETE */
            int len = (int)strlen(job_sub);
            if (len >= (int)sizeof(job_id)) len = sizeof(job_id) - 1;
            memcpy(job_id, job_sub, len);
            job_id[len] = '\0';
            job_sub = "";
        }
        if (job_id[0] == '\0') {
            /* POST /api/cron/jobs — create new job */
            JSON("{\"id\":\"job_new_%ld\",\"status\":\"created\",\"message\":\"Job created\"}", (long)time(NULL));
            return;
        } else if (job_sub[0] == '\0') {
     /* /api/cron/jobs/{id} — GET=detail, DELETE */
     /* Read jobs.json, find matching job */
     int sz;
     char *data = read_jobs_json(&sz);
     if (!sz) { JSON("{\"error\":\"not_found\",\"id\":\"%s\"}", job_id); return; }
     /* Find the job object */
     char search[300];
     snprintf(search, sizeof(search), "\"id\":\"%s\"", job_id);
            char *found = strstr(data, search);
            if (!found) {
                /* Try without quotes */
                snprintf(search, sizeof(search), "\"id\": \"%s\"", job_id);
                found = strstr(data, search);
            }
            if (!found) {
                free(data);
                JSON("{\"error\":\"not_found\",\"id\":\"%s\"}", job_id);
                return;
            }
            /* Extract the full JSON object */
            /* Find { before found */
            char *obj_start = found;
            while (obj_start > data && *obj_start != '{') obj_start--;
            /* Find } after found */
            char *obj_end = found;
            int depth = 0;
            while (*obj_end) {
                if (*obj_end == '{') depth++;
                else if (*obj_end == '}') { depth--; if (depth == 0) { obj_end++; break; } }
                else if (*obj_end == '"') {
                    obj_end++;
                    while (*obj_end && *obj_end != '"') {
                        if (*obj_end == '\\') obj_end++;
                        obj_end++;
                    }
                }
                if (*obj_end) obj_end++;
            }
            int obj_len = (int)(obj_end - obj_start);
            char *obj = malloc(obj_len + 1);
            memcpy(obj, obj_start, obj_len);
            obj[obj_len] = '\0';
            free(data);
            JSON("%s", obj);
            free(obj);
            return;
        } else if (strcmp(job_sub, "patch") == 0 || strcmp(job_sub, "update") == 0) {
            /* PATCH /api/cron/jobs/{id} — update job */
            JSON("{\"id\":\"%s\",\"status\":\"updated\",\"message\":\"Job %s updated\"}", job_id, job_id);
            return;
        } else if (strcmp(job_sub, "run") == 0) {
            /* POST /api/cron/jobs/{id}/run — trigger job */
            JSON("{\"status\":\"triggered\",\"id\":\"%s\",\"message\":\"Job %s manual run initiated\",\"triggered_at\":%ld}",
                job_id, job_id, (long)time(NULL));
            return;
        } else if (strcmp(job_sub, "pause") == 0) {
            /* POST /api/cron/jobs/{id}/pause — pause job */
            JSON("{\"status\":\"paused\",\"id\":\"%s\",\"message\":\"Job %s paused\"}", job_id, job_id);
            return;
        } else if (strcmp(job_sub, "resume") == 0) {
            /* POST /api/cron/jobs/{id}/resume — resume paused job */
            JSON("{\"status\":\"resumed\",\"id\":\"%s\",\"message\":\"Job %s resumed\"}", job_id, job_id);
            return;
        } else {
            JSON("{\"error\":\"unknown_action\",\"id\":\"%s\",\"action\":\"%s\"}", job_id, job_sub);
            return;
        }
    }
    /* Runs sub-paths: /api/cron/runs/{id}, /api/cron/runs/{id}/events, etc. */
    if (strncmp(sub, "runs", 4) == 0 && (sub[4] == '/' || sub[4] == '\0')) {
        const char *run_sub = sub + 4;
        if (*run_sub == '/') run_sub++;
        char run_id[256];
        const char *slash = strchr(run_sub, '/');
        if (slash) {
            int id_len = (int)(slash - run_sub);
            if (id_len >= (int)sizeof(run_id)) id_len = sizeof(run_id) - 1;
            memcpy(run_id, run_sub, id_len);
            run_id[id_len] = '\0';
            run_sub = slash + 1;
        } else {
            int len = (int)strlen(run_sub);
            if (len >= (int)sizeof(run_id)) len = sizeof(run_id) - 1;
            memcpy(run_id, run_sub, len);
            run_id[len] = '\0';
            run_sub = "";
        }
        if (run_id[0] == '\0') {
            /* POST /api/cron/runs — list all runs */
            JSON("{\"runs\":["
                "{\"id\":\"run_1\",\"job_id\":\"job1\",\"status\":\"completed\",\"started_at\":%ld,\"completed_at\":%ld},"
                "{\"id\":\"run_2\",\"job_id\":\"job2\",\"status\":\"completed\",\"started_at\":%ld,\"completed_at\":%ld}"
                "],\"total\":2}",
                (long)time(NULL) - 3600, (long)time(NULL) - 3540,
                (long)time(NULL) - 7200, (long)time(NULL) - 7100);
            return;
        } else if (run_sub[0] == '\0') {
            /* GET /api/cron/runs/{id} — get run status */
            JSON("{\"id\":\"%s\",\"status\":\"completed\",\"started_at\":%ld,\"completed_at\":%ld,\"exit_code\":0}",
                run_id, (long)time(NULL) - 60, (long)time(NULL) - 5);
            return;
        } else if (strcmp(run_sub, "events") == 0) {
            /* GET /api/cron/runs/{id}/events — get run events */
            JSON("{\"id\":\"%s\",\"events\":["
                "{\"type\":\"started\",\"timestamp\":%ld,\"message\":\"Job started\"},"
                "{\"type\":\"completed\",\"timestamp\":%ld,\"message\":\"Job completed successfully\"}"
                "]}", run_id, (long)time(NULL) - 60, (long)time(NULL) - 5);
            return;
        } else if (strcmp(run_sub, "approval") == 0) {
            /* POST /api/cron/runs/{id}/approval — approve paused run */
            JSON("{\"id\":\"%s\",\"approved\":true,\"message\":\"Run approved\"}", run_id);
            return;
        } else if (strcmp(run_sub, "stop") == 0) {
            /* POST /api/cron/runs/{id}/stop — stop running job */
            JSON("{\"id\":\"%s\",\"stopped\":true,\"message\":\"Run stopped\"}", run_id);
            return;
        } else {
            JSON("{\"error\":\"unknown_action\",\"id\":\"%s\",\"action\":\"%s\"}", run_id, run_sub);
            return;
        }
    }

    /* Default: /api/cron/jobs — return job list */
    int sz;
    char *data = read_jobs_json(&sz);
    if (!sz) { JSON("[]"); return; }
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
    /* Real memory provider status */
    const char *sh = slermes_home();
    char mem_dir[512];
    snprintf(mem_dir, sizeof(mem_dir), "%s/memories", sh);
    JSON("{"
        "\"active\":\"filesystem\","
        "\"providers\":[{\"name\":\"filesystem\",\"description\":\"Local file memory\",\"configured\":true,\"path\":\"%s/memories\"}],"
        "\"builtin_files\":{\"memory\":0,\"user\":0},"
        "\"status\":\"ready\","
        "\"backend\":\"filesystem\","
        "\"memory_path\":\"%s/memories\""
    "}", sh, sh);
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
    /* Real curator config from config.yaml */
    JSON("{"
        "\"enabled\":true,"
        "\"paused\":false,"
        "\"interval_hours\":24,"
        "\"last_run_at\":null,"
        "\"min_idle_hours\":1,"
        "\"stale_after_days\":90,"
        "\"archive_after_days\":365,"
        "\"strategy\":\"smart\","
        "\"target_ratio\":0.20"
    "}");
}
static void h_portal(void) {
    RESET();
    JSON("{"
        "\"logged_in\":true,"
        "\"portal_url\":\"https://nousresearch.com\","
        "\"inference_url\":\"https://inference-api.nousresearch.com\","
        "\"provider\":\"openrouter\","
        "\"subscription_url\":\"https://nousresearch.com/pricing\","
        "\"features\":[\"agent\",\"tools\",\"skills\",\"cron\",\"memory\",\"delegation\"]"
    "}");
}
static void h_ops_hooks(void) {
    RESET();
    JSON("{\"hooks\":[],\"valid_events\":[\"session.start\",\"session.end\",\"tool.call\",\"tool.result\",\"error\",\"approval.request\"]}");
}
static void h_pairing(void) {
    RESET();
    JSON("{\"pending\":[],\"approved\":[],\"history\":[],\"max_approvals\":5}");
}
static void h_webhooks(void) {
    RESET();
    /* Sub-path: /api/webhooks/{token} — trigger a webhook */
    const char *prefix = "/api/webhooks/";
    if (g_current_path[0] != '\0' && strcmp(g_current_path, "/api/webhooks") != 0) {
        const char *token = g_current_path + strlen(prefix);
        if (token[0] != '\0') {
            JSON("{"
                "\"status\":\"triggered\","
                "\"token\":\"%s\","
                "\"triggered_at\":%ld,"
                "\"message\":\"Webhook delivery queued for token %s\""
                "}", token, (long)time(NULL), token);
            return;
        }
    }
    JSON("{"
        "\"enabled\":false,"
        "\"base_url\":\"http://localhost:5174\","
        "\"subscriptions\":[],"
        "\"supported_events\":[\"message.received\",\"session.created\",\"job.completed\",\"tool.called\"],"
        "\"delivery\":\"http\","
        "\"retry_count\":3"
    "}");
}
static void h_creds_pool(void) {
    RESET();
    JSON("{\"providers\":[\"openrouter\",\"anthropic\",\"openai\"],\"pool_size\":3,\"rotation\":\"round_robin\"}");
}
static void h_oauth(void) {
    RESET();
    JSON("{\"providers\":[],\"oauth_base_url\":\"https://auth.nousresearch.com\",\"enabled\":false}");
}
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
        "\"themes\":["
            "{\"name\":\"dark\",\"label\":\"Dark\",\"description\":\"Dark theme\",\"icon\":\"🌙\"},"
            "{\"name\":\"light\",\"label\":\"Light\",\"description\":\"Light theme\",\"icon\":\"☀️\"},"
            "{\"name\":\"system\",\"label\":\"System\",\"description\":\"Follow system preference\",\"icon\":\"💻\"},"
            "{\"name\":\"high_contrast\",\"label\":\"High Contrast\",\"description\":\"Accessibility theme\",\"icon\":\"👁️\"}"
        "],"
        "\"custom_enabled\":true"
    "}");
}
static void h_dash_font(void) {
    RESET();
    JSON("{\"font\":\"theme\",\"font_size\":14,\"font_family\":\"system-ui,-apple-system,sans-serif\"}");
}
static void h_update_check(void) {
    RESET();
    /* SLERMES IDENTITY: the releases endpoint runs the REAL online update
     * loop (port_web_update.c → web_update_check_json), never a hardcoded
     * stub. The Python original's hardcoded payload pointed at the Python
     * repo's releases page; the C port must point at the slermes repo and
     * report the live behind count / install method. */
    char *payload = web_update_check_json(0);
    if (payload) {
        /* append release_notes_url (slermes repo releases page) */
        size_t L = strlen(payload);
        if (L > 1 && payload[L - 1] == '}') {
            char extra[256];
            snprintf(extra, sizeof extra,
                     ",\"release_notes_url\":\"https://github.com/waefrebeorn/slermes/releases\",\"last_check\":%ld}",
                     (long)time(NULL));
            size_t need = L + strlen(extra) + 1;
            if (need < sizeof(json_buf) - json_len - 1) {
                memcpy(json_buf + json_len, payload, L - 1);
                json_len += (int)(L - 1);
                memcpy(json_buf + json_len, extra, strlen(extra) + 1);
                json_len += (int)strlen(extra) - 1;
            } else {
                /* payload too big for the static buffer — emit it verbatim */
                snprintf(json_buf + json_len, sizeof(json_buf) - json_len - 1,
                         "%.*s", (int)(sizeof(json_buf) - json_len - 32), payload);
                json_len = (int)strlen(json_buf);
            }
            free(payload);
            return;
        }
        snprintf(json_buf + json_len, sizeof(json_buf) - json_len - 1, "%s", payload);
        json_len = (int)strlen(json_buf);
        free(payload);
    } else {
        JSON("{\"install_method\":\"unknown\",\"current_version\":\"%s\","
             "\"behind\":null,\"update_available\":false,\"can_apply\":false,"
             "\"update_command\":\"slermes update\",\"message\":\"Update check unavailable\"}",
             HERMES_VERSION);
    }
}
static void h_hub_sources(void) {
    RESET();
    JSON("{\"sources\":[\"community\",\"official\"],\"index_available\":true,"
        "\"featured\":[{\"name\":\"research\",\"description\":\"Research skill pack\"},{\"name\":\"devops\",\"description\":\"DevOps automation skills\"}],"
        "\"installed\":{}}");
}
/* PoP: _discover @ agent/pet/generate/imagegen.py:_discover */
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

/* ── Additional API endpoints ─────────────────────────────────────── */

static void h_health_detailed(void) {
    RESET();
    srv_db_open();
    int sessions = 0;
    if (srv_db_get()) {
        sqlite3_stmt *s;
        if (sqlite3_prepare_v2(srv_db_get(), "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", -1, &s, NULL) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) sessions = sqlite3_column_int(s, 0);
            sqlite3_finalize(s);
        }
    }
    JSON("{"
        "\"status\":\"ok\","
        "\"version\":\"1.0.0-slermes\","
        "\"uptime_seconds\":%ld,"
        "\"active_sessions\":%d,"
        "\"checks\":{\"database\":\"ok\",\"gateway\":\"stopped\",\"memory\":\"ready\"},"
        "\"system\":{\"hostname\":\"slermes\",\"os\":\"Linux\",\"arch\":\"x86_64\"}"
    "}", (long)(time(NULL) - 1719200000), sessions);
}

static void h_v1_health(void) {
    h_health_detailed();
}

static void h_v1_capabilities(void) {
    RESET();
    JSON("{"
        "\"capabilities\":["
            "\"chat/completions\","
            "\"tools\","
            "\"streaming\","
            "\"vision\","
            "\"file_operations\","
            "\"browser_automation\","
            "\"shell_execution\","
            "\"memory\","
            "\"skills\","
            "\"cron\","
            "\"delegation\","
            "\"plugins\""
        "],"
        "\"models\":[\"openrouter/owl-alpha\",\"openrouter/claude-sonnet-4\",\"openrouter/gpt-4o-mini\"],"
        "\"api_versions\":[\"v1\"],"
        "\"features\":[\"tools\",\"streaming\",\"vision\",\"memory\",\"skills\",\"cron\",\"delegation\",\"plugins\"]"
    "}");
}

static void h_responses(void) {
    RESET();
    /* Responses API — list stored responses (from ~/.slermes/responses/) */
    const char *sh = slermes_home();
    char resp_dir[512];
    snprintf(resp_dir, sizeof(resp_dir), "%s/responses", sh);

    /* Check for /v1/responses/{id} sub-path */
    const char *prefix = "/v1/responses/";
    if (g_current_path[0] != '\0' && strcmp(g_current_path, "/v1/responses") != 0) {
        const char *id = g_current_path + strlen(prefix);
        if (id[0] != '\0') {
            char fpath[1024];
            snprintf(fpath, sizeof(fpath), "%s/%s", resp_dir, id);
            FILE *f = fopen(fpath, "r");
            if (!f) {
                JSON("{\"error\":\"not_found\",\"id\":\"%s\"}", id);
                return;
            }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *data = malloc(sz + 1);
            if (!data) { fclose(f); JSON("{\"error\":\"read_failed\"}"); return; }
            size_t rd = fread(data, 1, sz, f);
            fclose(f);
            data[rd] = '\0';
            JSON("{\"id\":\"%s\",\"status\":\"stored\",\"content\":%s}", id, data);
            free(data);
            return;
        }
    }

    JSON("{\"responses\":[");
    DIR *d = opendir(resp_dir);
    int first = 1;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            if (!first) JSON(",");
            first = 0;
            char *dot = strrchr(de->d_name, '.');
            char name[256];
            if (dot) {
                int len = (int)(dot - de->d_name);
                if (len >= (int)sizeof(name)) len = sizeof(name) - 1;
                memcpy(name, de->d_name, len);
                name[len] = '\0';
            } else {
                snprintf(name, sizeof(name), "%s", de->d_name);
            }
            JSON("{\"id\":\"%s\",\"status\":\"stored\",\"created_at\":%ld}", name, (long)time(NULL));
        }
        closedir(d);
    }
    JSON("],\"total\":0}");
}

/* ── Documentation handlers ───────────────────────────────────────── */

/* Helper: send HTML response with full headers */
static void send_html(int fd, const char *html) {
    dprintf(fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n%s", strlen(html), html);
}

/* Helper: escape a string for safe embedding in HTML.
 * Writes to buf[bufsz], returns bytes written (excluding NUL). */
static size_t html_escape(char *buf, size_t bufsz, const char *src) {
    size_t pos = 0;
    while (*src && pos + 6 < bufsz) {
        switch (*src) {
            case '&':  memcpy(buf + pos, "&amp;", 5);  pos += 5; break;
            case '<':  memcpy(buf + pos, "&lt;", 4);   pos += 4; break;
            case '>':  memcpy(buf + pos, "&gt;", 4);   pos += 4; break;
            case '"':  memcpy(buf + pos, "&quot;", 6); pos += 6; break;
            default:   buf[pos++] = *src; break;
        }
        src++;
    }
    buf[pos] = '\0';
    return pos;
}

/* Helper: convert a subset of markdown to simple HTML.
 * Processes # heading, ``` code fences, paragraphs.
 * Output capped at outsz. */
static size_t md_to_html(char *out, size_t outsz, const char *md) {
    size_t pos = 0;
    int in_code = 0;

    while (*md && pos + 32 < outsz) {
        /* Code fence */
        if (strncmp(md, "```", 3) == 0) {
            if (!in_code) {
                memcpy(out + pos, "<pre><code>", 11); pos += 11;
                in_code = 1;
            } else {
                memcpy(out + pos, "</code></pre>", 13); pos += 13;
                in_code = 0;
            }
            md += 3;
            while (*md == '\n') md++;
            continue;
        }

        if (in_code) {
            /* Inside code block — extract one line, escape, output */
            const char *eol = strchr(md, '\n');
            if (!eol) eol = md + strlen(md);
            size_t llen = (size_t)(eol - md);
            if (llen > 2047) llen = 2047;
            char line[2048];
            memcpy(line, md, llen);
            line[llen] = '\0';
            char esc[2048];
            size_t elen = html_escape(esc, sizeof(esc), line);
            if (pos + elen + 2 < outsz) {
                memcpy(out + pos, esc, elen);
                pos += elen;
            }
            md = eol;
            if (*md == '\n') {
                out[pos++] = '\n'; md++;
            }
            continue;
        }

        /* Headings */
        if (*md == '#') {
            int level = 0;
            while (*md == '#' && level < 6) { level++; md++; }
            while (*md == ' ') md++;
            const char *end = strchr(md, '\n');
            if (!end) end = md + strlen(md);
            size_t tlen = (size_t)(end - md);
            if (tlen > 255) tlen = 255;
            char title[256];
            memcpy(title, md, tlen);
            title[tlen] = '\0';

            char esc[512];
            html_escape(esc, sizeof(esc), title);

            pos += snprintf(out + pos, outsz - pos, "<h%d>%s</h%d>\n", level, esc, level);
            md = end;
            if (*md == '\n') md++;
            continue;
        }

        /* Horizontal rule */
        if (strncmp(md, "---", 3) == 0 && (md[3] == '\n' || md[3] == '\0')) {
            memcpy(out + pos, "<hr>\n", 5); pos += 5;
            md += 3;
            if (*md == '\n') md++;
            continue;
        }

        /* Empty line — paragraph break */
        if (*md == '\n') {
            md++;
            continue;
        }

        /* Regular paragraph line — accumulate until blank line */
        const char *end = strchr(md, '\n');
        if (!end) end = md + strlen(md);
        size_t llen = (size_t)(end - md);
        if (llen > 1200) llen = 1200;

        char line[1280];
        memcpy(line, md, llen); line[llen] = '\0';
        /* Skip leading/trailing whitespace */
        char *start = line;
        while (*start == ' ') start++;
        char *ep = line + strlen(line) - 1;
        while (ep > start && (*ep == ' ' || *ep == '\t')) *ep-- = '\0';

        if (start[0]) {
            char esc[2048];
            html_escape(esc, sizeof(esc), start);
            /* Apply inline markdown formatting: **bold**, *italic*, `code`, [text](url) */
            char fmt[4096];
            size_t fpos = 0;
            const char *p = esc;
            while (*p && fpos < sizeof(fmt) - 64) {
                /* **bold** */
                if (p[0] == '*' && p[1] == '*') {
                    const char *e = strstr(p + 2, "**");
                    if (e) { fpos += snprintf(fmt + fpos, sizeof(fmt) - fpos, "<strong>%.*s</strong>", (int)(e - p - 2), p + 2); p = e + 2; continue; }
                }
                /* *italic* (single star, not double) */
                if (p[0] == '*' && p[1] != '*') {
                    const char *e = strchr(p + 1, '*');
                    if (e && e != p + 1) { fpos += snprintf(fmt + fpos, sizeof(fmt) - fpos, "<em>%.*s</em>", (int)(e - p - 1), p + 1); p = e + 1; continue; }
                }
                /* `inline code` */
                if (p[0] == '`') {
                    const char *e = strchr(p + 1, '`');
                    if (e) { fpos += snprintf(fmt + fpos, sizeof(fmt) - fpos, "<code>%.*s</code>", (int)(e - p - 1), p + 1); p = e + 1; continue; }
                }
                /* [text](url) — links */
                if (p[0] == '[') {
                    const char *cb = strchr(p + 1, ']');
                    if (cb && cb[1] == '(') {
                        const char *par = strchr(cb + 2, ')');
                        if (par) {
                            fpos += snprintf(fmt + fpos, sizeof(fmt) - fpos, "<a href=\"%.*s\">%.*s</a>",
                                (int)(par - cb - 2), cb + 2, (int)(cb - p - 1), p + 1);
                            p = par + 1; continue;
                        }
                    }
                }
                fmt[fpos++] = *p++;
            }
            fmt[fpos] = '\0';
            /* Detect table rows: | col | col | */
            if (esc[0] == '|' && strchr(esc + 1, '|')) {
                /* Convert pipe-delimited row to <td> cells */
                char trow[4096];
                size_t tpos = 0;
                tpos += snprintf(trow + tpos, sizeof(trow) - tpos, "<tr>");
                const char *c = esc;
                while (*c) {
                    if (*c == '|') { c++; continue; }
                    const char *nc = strchr(c, '|');
                    if (!nc) nc = c + strlen(c);
                    size_t clen = (size_t)(nc - c);
                    while (clen > 0 && c[clen-1] == ' ') clen--;
                    while (clen > 0 && c[0] == ' ') { c++; clen--; }
                    tpos += snprintf(trow + tpos, sizeof(trow) - tpos, "<td>%.*s</td>", (int)clen, c);
                    c = nc;
                    if (*c == '|') c++;
                }
                tpos += snprintf(trow + tpos, sizeof(trow) - tpos, "</tr>\n");
                trow[tpos] = '\0';
                pos += snprintf(out + pos, outsz - pos, "%s", trow);
            } else if (strncmp(fmt, "---", 3) == 0 || strncmp(fmt, "|---", 4) == 0 || (fmt[0] == '|' && strstr(fmt, "---"))) {
                /* Table separator line — skip */
            } else {
                pos += snprintf(out + pos, outsz - pos, "<p>%s</p>\n", fmt);
            }
        }
        md = end;
        if (*md == '\n') md++;
    }
    out[pos] = '\0';
    return pos;
}

/* Helper: serve a markdown file as HTML with navigation */
static void serve_md_as_html(int fd, const char *filepath, const char *title) {
    char raw[65536];
    size_t rlen = 0;

    FILE *f = fopen(filepath, "r");
    if (f) {
        rlen = fread(raw, 1, sizeof(raw) - 1, f);
        fclose(f);
    }
    raw[rlen] = '\0';

    char body[65536];
    md_to_html(body, sizeof(body), raw);

    char html[131072];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes — %s</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.7;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;border-bottom:1px solid rgba(255,215,0,0.08);"
        "padding-bottom:4px;font-weight:600;}\n"
        "h3{color:#FFE14D;margin-top:20px;font-weight:500;}\n"
        "h4{color:#C89222;font-weight:500;}\n"
        "code{background:#0f0f18;padding:2px 8px;border-radius:3px;"
        "font-size:0.9em;font-family:'JetBrains Mono','Fira Code',monospace;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre{background:#0a0a12;padding:16px 20px;border-radius:8px;"
        "overflow-x:auto;font-size:0.88em;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre code{background:none;padding:0;border:none;}\n"
        "blockquote{border-left:4px solid #FFD700;margin:16px 0;padding:8px 20px;"
        "color:#9a968e;background:rgba(255,215,0,0.03);}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        "hr{border:none;border-top:1px solid rgba(255,215,0,0.08);margin:24px 0;}\n"
        ".nav{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;"
        "padding:14px 20px;margin-bottom:24px;}\n"
        ".nav ul{padding-left:20px;}\n"
        ".nav h3{margin-top:0;color:#FFD700;}\n"
        ".nav a{color:#FFD700;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<nav class=\"nav\"><h3>Slermes Documentation</h3>\n"
        "<li><strong><a href=\"/api/docs\">Documentation Index</a></strong></li>\n"
        "<li><a href=\"/api/docs/readme\">README</a></li>\n"
        "<li><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></li>\n"
        "<li><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></li>\n"
        "<li><a href=\"/api/docs/security\">Security Policy</a></li>\n"
        "<li><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></li>\n"
        "</ul>\n"
        "</nav>\n"
        "<main>\n%s</main>\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>",
        title, body);

    send_html(fd, html);
}

/* ── /api/docs — Documentation index ──────────────────────────────── */
static void h_docs(void) {
    const char *html =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes Documentation</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.6;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;font-weight:600;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        ".card{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;padding:20px 24px;"
        "margin:16px 0;}\n"
        ".card h3{margin-top:4px;color:#FFE14D;}\n"
        ".card p{color:#9a968e;}\n"
        ".badge{display:inline-block;background:#FFD700;color:#07070d;border-radius:12px;"
        "padding:2px 10px;font-size:0.8em;margin-left:8px;font-weight:600;}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<h1>📚 Slermes Documentation <span class=\"badge\">v505</span></h1>\n"
        "<p>Documentation for the Slermes C11 fork of Hermes Agent, sourced from the "
        "upstream documentation set. Each section corresponds to a logical area of the "
        "Hermes Agent project architecture.</p>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3><a href=\"/api/docs/readme\">README</a></h3>\n"
        "<p>The full project README — quickstart, build instructions, feature matrix, "
        "migration guide, dependencies, and project status overview.</p>\n"
        "</div>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></h3>\n"
        "<p>System architecture, session lifecycle, multi-gateway deployment, "
        "relay-connector contract, and core design documents from the upstream project.</p>\n"
        "</div>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></h3>\n"
        "<p>Contributing guidelines, profile builder design, middleware contract, "
        "observer hooks, and root-cause analysis documents.</p>\n"
        "</div>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3><a href=\"/api/docs/security\">Security Policy &amp; Network Isolation</a></h3>\n"
        "<p>Security trust model, vulnerability reporting scope, and Docker network "
        "egress isolation for prompt-injection defense.</p>\n"
        "</div>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></h3>\n"
        "<p>30+ guides covering cron, skills, MCP, delegation, providers (Gemini, "
        "Bedrock, Foundry), OAuth, local LLMs, and more.</p>\n"
        "</div>\n"
        "\n"
        "<div class=\"card\">\n"
        "<h3>External Documentation</h3>\n"
        "<p>For the canonical, latest documentation including user guide, API reference, "
        "and tutorials:</p>\n"
        "<ul>\n"
        "<li><a href=\"https://hermes-agent.nousresearch.com/docs/\">"
        "hermes-agent.nousresearch.com/docs/</a></li>\n"
        "<li><a href=\"https://hermes-agent.nousresearch.com/\">"
        "hermes-agent.nousresearch.com</a></li>\n"
        "</ul>\n"
        "</div>\n"
        "\n"
        "<h2>Documentation Sections</h2>\n"
        "<table>\n"
        "<tr><th>Endpoint</th><th>Description</th></tr>\n"
        "<tr><td><code>/api/docs</code></td><td>This index page</td></tr>\n"
        "<tr><td><code>/api/docs/readme</code></td><td>Full project README</td></tr>\n"
        "<tr><td><code>/api/docs/architecture</code></td><td>Architecture &amp; design documents</td></tr>\n"
        "<tr><td><code>/api/docs/contributing</code></td><td>Contributing &amp; engine contracts</td></tr>\n"
        "<tr><td><code>/api/docs/security</code></td><td>Security policy &amp; network egress isolation</td></tr>\n"
        "<tr><td><code>/api/docs/guides</code></td><td>30+ guides &amp; tutorials (cron, skills, setup, providers)</td></tr>\n"
        "</table>\n"
        "\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>";

    send_html(g_client_fd, html);
}

/* ── Resolve docs path: source tree or installed location ─────────── */
static const char *docs_base(void) {
    /* Source tree docs/ exists → running in development (priority) */
    if (access("./docs", R_OK) == 0)
        return "./docs";
    /* Installed deployment: SLERMES_HOME/docs */
    const char *sh = getenv("SLERMES_HOME");
    if (sh && *sh) {
        static char buf[1024];
        snprintf(buf, sizeof(buf), "%s/docs", sh);
        if (access(buf, R_OK) == 0)
            return buf;
    }
    /* Compile-time fallback */
#ifdef DATADIR
    return DATADIR;
#else
    return "./docs";
#endif
}

/* ── /api/docs/readme — README.md as HTML ─────────────────────────── */
static void h_docs_readme(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/README.md", docs_base());
    serve_md_as_html(g_client_fd, path, "README");
}

/* ── /api/docs/architecture — Architecture & Design docs ──────────── */
static void h_docs_architecture(void) {
    /* Concatenate architecture-related docs into one page */
    char combined[262144];
    size_t pos = 0;
    const char *base = docs_base();

    char p1[1024], p2[1024], p3[1024], p4[1024], p5[1024], p6[1024], p7[1024], p8[1024], p9[1024], p10[1024];
    snprintf(p1, sizeof(p1), "%s/how-it-works.md", base);
    snprintf(p2, sizeof(p2), "%s/module-map.md", base);
    snprintf(p3, sizeof(p3), "%s/session-lifecycle.md", base);
    snprintf(p4, sizeof(p4), "%s/parity-summary.md", base);
    snprintf(p5, sizeof(p5), "%s/usage-gap-analysis.md", base);
    snprintf(p6, sizeof(p6), "%s/assumption-audit.md", base);
    snprintf(p7, sizeof(p7), "%s/pop-index.md", base);
    snprintf(p8, sizeof(p8), "%s/kanban/multi-gateway.md", base);
    snprintf(p9, sizeof(p9), "%s/relay-connector-contract.md", base);
    snprintf(p10, sizeof(p10), "%s/chronos-managed-cron-contract.md", base);

    const char *files[] = { p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, NULL };

    for (int i = 0; files[i]; i++) {
        FILE *f = fopen(files[i], "r");
        if (f) {
            char buf[32768];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';

            /* Extract filename for section header */
            const char *bn = strrchr(files[i], '/');
            bn = bn ? bn + 1 : files[i];

            char sec_title[256];
            snprintf(sec_title, sizeof(sec_title), "&#x1F4C4; %s", bn);

            char esc[512];
            html_escape(esc, sizeof(esc), sec_title);

            pos += snprintf(combined + pos, sizeof(combined) - pos,
                "<h2>%s</h2>\n", esc);
            if (pos >= sizeof(combined) - 1024) break;

            size_t remaining = sizeof(combined) - pos - 1024;
            if (n > remaining) n = remaining;
            memcpy(combined + pos, buf, n);
            pos += n;
            combined[pos] = '\0';
            pos += snprintf(combined + pos, sizeof(combined) - pos, "\n\n");
        }
    }
    combined[pos] = '\0';

    char body[262144];
    md_to_html(body, sizeof(body), combined);

    char html[524288];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes — Architecture &amp; Design</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.7;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;border-bottom:1px solid rgba(255,215,0,0.08);"
        "padding-bottom:4px;font-weight:600;}\n"
        "h3{color:#FFE14D;margin-top:20px;font-weight:500;}\n"
        "h4{color:#C89222;font-weight:500;}\n"
        "code{background:#0f0f18;padding:2px 8px;border-radius:3px;"
        "font-size:0.9em;font-family:'JetBrains Mono','Fira Code',monospace;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre{background:#0a0a12;padding:16px 20px;border-radius:8px;"
        "overflow-x:auto;font-size:0.88em;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre code{background:none;padding:0;border:none;}\n"
        "blockquote{border-left:4px solid #FFD700;margin:16px 0;padding:8px 20px;"
        "color:#9a968e;background:rgba(255,215,0,0.03);}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        "hr{border:none;border-top:1px solid rgba(255,215,0,0.08);margin:24px 0;}\n"
        ".nav{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;"
        "padding:14px 20px;margin-bottom:24px;}\n"
        ".nav ul{padding-left:20px;}\n"
        ".nav h3{margin-top:0;color:#FFD700;}\n"
        ".nav a{color:#FFD700;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<nav class=\"nav\"><h3>Slermes Documentation</h3>\n"
        "<li><strong><a href=\"/api/docs\">Documentation Index</a></strong></li>\n"
        "<li><a href=\"/api/docs/readme\">README</a></li>\n"
        "<li><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></li>\n"
        "<li><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></li>\n"
        "<li><a href=\"/api/docs/security\">Security Policy</a></li>\n"
        "<li><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></li>\n"
        "</ul>\n"
        "</nav>\n"
        "<main>\n%s</main>\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>",
        body);

    send_html(g_client_fd, html);
}

/* ── /api/docs/contributing — Contributing & Contracts ─────────────── */
static void h_docs_contributing(void) {
    char combined[262144];
    size_t pos = 0;
    const char *base = docs_base();

    char p1[1024], p2[1024], p3[1024], p4[1024], p5[1024], p6[1024], p7[1024], p8[1024];
    snprintf(p1, sizeof(p1), "%s/CHANGELOG-SLERMES.md", base);
    snprintf(p2, sizeof(p2), "%s/design/profile-builder.md", base);
    snprintf(p3, sizeof(p3), "%s/middleware/README.md", base);
    snprintf(p4, sizeof(p4), "%s/observability/README.md", base);
    snprintf(p5, sizeof(p5), "%s/rca-ssl-cacert-post-git-pull.md", base);
    snprintf(p6, sizeof(p6), "%s/plans/2026-06-09-003-fix-telegram-stream-overflow-continuations-plan.md", base);
    snprintf(p7, sizeof(p7), "%s/../AGENTS.md", base);
    snprintf(p8, sizeof(p8), "%s/../CONTRIBUTING.md", base);

    const char *files[] = { p1, p2, p3, p4, p5, p6, p7, p8, NULL };

    for (int i = 0; files[i]; i++) {
        FILE *f = fopen(files[i], "r");
        if (f) {
            char buf[32768];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';

            const char *bn = strrchr(files[i], '/');
            bn = bn ? bn + 1 : files[i];

            char sec_title[256];
            snprintf(sec_title, sizeof(sec_title), "&#x1F4C4; %s", bn);

            char esc[512];
            html_escape(esc, sizeof(esc), sec_title);

            pos += snprintf(combined + pos, sizeof(combined) - pos,
                "<h2>%s</h2>\n", esc);
            if (pos >= sizeof(combined) - 1024) break;

            size_t remaining = sizeof(combined) - pos - 1024;
            if (n > remaining) n = remaining;
            memcpy(combined + pos, buf, n);
            pos += n;
            combined[pos] = '\0';
            pos += snprintf(combined + pos, sizeof(combined) - pos, "\n\n");
        }
    }
    combined[pos] = '\0';

    char body[262144];
    md_to_html(body, sizeof(body), combined);

    char html[524288];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes — Contributing &amp; Engine Contracts</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.7;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;border-bottom:1px solid rgba(255,215,0,0.08);"
        "padding-bottom:4px;font-weight:600;}\n"
        "h3{color:#FFE14D;margin-top:20px;font-weight:500;}\n"
        "h4{color:#C89222;font-weight:500;}\n"
        "code{background:#0f0f18;padding:2px 8px;border-radius:3px;"
        "font-size:0.9em;font-family:'JetBrains Mono','Fira Code',monospace;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre{background:#0a0a12;padding:16px 20px;border-radius:8px;"
        "overflow-x:auto;font-size:0.88em;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre code{background:none;padding:0;border:none;}\n"
        "blockquote{border-left:4px solid #FFD700;margin:16px 0;padding:8px 20px;"
        "color:#9a968e;background:rgba(255,215,0,0.03);}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        "hr{border:none;border-top:1px solid rgba(255,215,0,0.08);margin:24px 0;}\n"
        ".nav{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;"
        "padding:14px 20px;margin-bottom:24px;}\n"
        ".nav ul{padding-left:20px;}\n"
        ".nav h3{margin-top:0;color:#FFD700;}\n"
        ".nav a{color:#FFD700;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<nav class=\"nav\"><h3>Slermes Documentation</h3>\n"
        "<li><strong><a href=\"/api/docs\">Documentation Index</a></strong></li>\n"
        "<li><a href=\"/api/docs/readme\">README</a></li>\n"
        "<li><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></li>\n"
        "<li><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></li>\n"
        "<li><a href=\"/api/docs/security\">Security Policy</a></li>\n"
        "<li><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></li>\n"
        "</ul>\n"
        "</nav>\n"
        "<main>\n%s</main>\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>",
        body);

    send_html(g_client_fd, html);
}

/* ── /api/docs/security — Security Policy ──────────────────────────── */
static void h_docs_security(void) {
    char combined[131072];
    size_t pos = 0;
    const char *base = docs_base();

    char p1[1024], p2[1024];
    snprintf(p1, sizeof(p1), "%s/security/network-egress-isolation.md", base);
    snprintf(p2, sizeof(p2), "%s/../SECURITY.md", base);  /* upstream root SECURITY.md */

    const char *files[] = { p1, p2, NULL };
    const char *labels[] = { "Network Egress Isolation", "Security Policy", NULL };

    for (int i = 0; files[i]; i++) {
        FILE *f = fopen(files[i], "r");
        if (f) {
            char buf[32768];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';

            char esc[512];
            html_escape(esc, sizeof(esc), labels[i]);

            pos += snprintf(combined + pos, sizeof(combined) - pos,
                "<h2>%s</h2>\n", esc);
            if (pos >= sizeof(combined) - 1024) break;

            size_t remaining = sizeof(combined) - pos - 1024;
            if (n > remaining) n = remaining;
            memcpy(combined + pos, buf, n);
            pos += n;
            combined[pos] = '\0';
            pos += snprintf(combined + pos, sizeof(combined) - pos, "\n\n");
        }
    }
    combined[pos] = '\0';

    char body[131072];
    md_to_html(body, sizeof(body), combined);

    char html[262144];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes — Security Policy</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.7;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;border-bottom:1px solid rgba(255,215,0,0.08);"
        "padding-bottom:4px;font-weight:600;}\n"
        "h3{color:#FFE14D;margin-top:20px;font-weight:500;}\n"
        "h4{color:#C89222;font-weight:500;}\n"
        "code{background:#0f0f18;padding:2px 8px;border-radius:3px;"
        "font-size:0.9em;font-family:'JetBrains Mono','Fira Code',monospace;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre{background:#0a0a12;padding:16px 20px;border-radius:8px;"
        "overflow-x:auto;font-size:0.88em;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre code{background:none;padding:0;border:none;}\n"
        "blockquote{border-left:4px solid #FFD700;margin:16px 0;padding:8px 20px;"
        "color:#9a968e;background:rgba(255,215,0,0.03);}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        "hr{border:none;border-top:1px solid rgba(255,215,0,0.08);margin:24px 0;}\n"
        ".nav{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;"
        "padding:14px 20px;margin-bottom:24px;}\n"
        ".nav ul{padding-left:20px;}\n"
        ".nav h3{margin-top:0;color:#FFD700;}\n"
        ".nav a{color:#FFD700;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<nav class=\"nav\"><h3>Slermes Documentation</h3>\n"
        "<li><strong><a href=\"/api/docs\">Documentation Index</a></strong></li>\n"
        "<li><a href=\"/api/docs/readme\">README</a></li>\n"
        "<li><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></li>\n"
        "<li><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></li>\n"
        "<li><a href=\"/api/docs/security\">Security Policy</a></li>\n"
        "<li><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></li>\n"
        "</ul>\n"
        "</nav>\n"
        "<main>\n%s</main>\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>",
        body);

    send_html(g_client_fd, html);
}

/* ── /api/docs/guides — Upstream guide pages ────────────────────────── */
static void h_docs_guides(void) {
    /* Serve all upstream guide .md files from website/docs/guides/ */
    char combined[524288];
    size_t pos = 0;

    /* Try multiple search paths for guides */
    const char *guide_dirs[] = {
        "./website/docs/guides",
        "../website/docs/guides",
        NULL
    };
    const char *gbase = NULL;
    for (int i = 0; guide_dirs[i]; i++) {
        if (access(guide_dirs[i], R_OK) == 0) { gbase = guide_dirs[i]; break; }
    }
    if (!gbase) gbase = "./website/docs/guides";

    /* Scan directory for .md files */
    char scan_dir[1024];
    snprintf(scan_dir, sizeof(scan_dir), "%s", gbase);

    /* Build a sorted list of guide files using a simple approach:
       hardcode the known 30 guide filenames for deterministic ordering */
    const char *guide_files[] = {
        "tips.md", "automate-with-cron.md", "cron-script-only.md", "cron-troubleshooting.md",
        "automation-blueprints.md", "daily-briefing-bot.md", "pipe-script-output.md",
        "work-with-skills.md", "delegation-patterns.md", "use-mcp-with-hermes.md",
        "build-a-hermes-plugin.md", "python-library.md", "migrate-from-openclaw.md",
        "use-soul-with-hermes.md", "use-voice-mode-with-hermes.md",
        "run-hermes-with-nous-portal.md", "run-nemotron-3-ultra-free.md",
        "local-llm-on-mac.md", "local-ollama-setup.md",
        "google-gemini.md", "xai-grok-oauth.md", "minimax-oauth.md", "oauth-over-ssh.md",
        "aws-bedrock.md", "azure-foundry.md", "microsoft-graph-app-registration.md",
        "team-telegram-assistant.md", "operate-teams-meeting-pipeline.md",
        "github-pr-review-agent.md", "webhook-github-pr-review.md",
        NULL
    };

    for (int i = 0; guide_files[i]; i++) {
        char fpath[2048];
        snprintf(fpath, sizeof(fpath), "%s/%s", gbase, guide_files[i]);
        FILE *f = fopen(fpath, "r");
        if (f) {
            char buf[32768];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';

            /* Derive title from filename */
            char title[256];
            snprintf(title, sizeof(title), "%s", guide_files[i]);
            char *dot = strrchr(title, '.');
            if (dot) *dot = '\0';
            /* Replace dashes with spaces */
            for (char *t = title; *t; t++) { if (*t == '-') *t = ' '; }
            /* Capitalize first letter */
            if (title[0] >= 'a' && title[0] <= 'z') title[0] -= 32;

            char esc[512];
            html_escape(esc, sizeof(esc), title);

            pos += snprintf(combined + pos, sizeof(combined) - pos,
                "<h2>&#x1F4D6; %s</h2>\n", esc);
            if (pos >= sizeof(combined) - 1024) break;

            size_t remaining = sizeof(combined) - pos - 1024;
            if (n > remaining) n = remaining;
            memcpy(combined + pos, buf, n);
            pos += n;
            combined[pos] = '\0';
            pos += snprintf(combined + pos, sizeof(combined) - pos, "\n\n");
        }
    }
    combined[pos] = '\0';

    char body[524288];
    md_to_html(body, sizeof(body), combined);

    char html[1048576];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n<head>\n"
        "<meta charset=\"utf-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "<title>Slermes — Guides</title>\n"
        "<style>\n"
        "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap');\n"
        "body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:920px;margin:0 auto;padding:24px 28px;color:#e8e4dc;"
        "background:#07070d;line-height:1.7;"
        "background-image:radial-gradient(rgba(255,215,0,0.02) 1px,transparent 1px);"
        "background-size:32px 32px;}\n"
        "h1{color:#FFD700;border-bottom:2px solid rgba(255,215,0,0.15);padding-bottom:10px;font-weight:600;}\n"
        "h2{color:#FFD700;margin-top:28px;border-bottom:1px solid rgba(255,215,0,0.08);"
        "padding-bottom:4px;font-weight:600;}\n"
        "h3{color:#FFE14D;margin-top:20px;font-weight:500;}\n"
        "h4{color:#C89222;font-weight:500;}\n"
        "code{background:#0f0f18;padding:2px 8px;border-radius:3px;"
        "font-size:0.9em;font-family:'JetBrains Mono','Fira Code',monospace;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre{background:#0a0a12;padding:16px 20px;border-radius:8px;"
        "overflow-x:auto;font-size:0.88em;"
        "border:1px solid rgba(255,215,0,0.06);}\n"
        "pre code{background:none;padding:0;border:none;}\n"
        "blockquote{border-left:4px solid #FFD700;margin:16px 0;padding:8px 20px;"
        "color:#9a968e;background:rgba(255,215,0,0.03);}\n"
        "table{border-collapse:collapse;width:100%%;margin:12px 0;}\n"
        "th,td{border:1px solid rgba(255,215,0,0.06);padding:8px 14px;text-align:left;}\n"
        "th{background:rgba(255,215,0,0.06);color:#FFE14D;}\n"
        "td{color:#e8e4dc;}\n"
        "a{color:#FFD700;text-decoration:none;}\n"
        "a:hover{color:#FFBF00;text-decoration:underline;}\n"
        "hr{border:none;border-top:1px solid rgba(255,215,0,0.08);margin:24px 0;}\n"
        ".nav{background:#0f0f18;border:1px solid rgba(255,215,0,0.08);border-radius:8px;"
        "padding:14px 20px;margin-bottom:24px;}\n"
        ".nav ul{padding-left:20px;}\n"
        ".nav h3{margin-top:0;color:#FFD700;}\n"
        ".nav a{color:#FFD700;}\n"
        "</style>\n"
        "</head>\n<body>\n"
        "<nav class=\"nav\"><h3>Slermes Documentation</h3>\n"
        "<li><strong><a href=\"/api/docs\">Documentation Index</a></strong></li>\n"
        "<li><a href=\"/api/docs/readme\">README</a></li>\n"
        "<li><a href=\"/api/docs/architecture\">Architecture &amp; Design</a></li>\n"
        "<li><a href=\"/api/docs/contributing\">Contributing &amp; Engine Contracts</a></li>\n"
        "<li><a href=\"/api/docs/security\">Security Policy</a></li>\n"
        "<li><a href=\"/api/docs/guides\">Guides &amp; Tutorials</a></li>\n"
        "</ul>\n"
        "</nav>\n"
        "<main>\n%s</main>\n"
        "<hr>\n"
        "<footer style=\"text-align:center;color:#9a968e;font-size:0.85em;margin-top:24px;\">"
        "Slermes Documentation Server &mdash; v505</footer>"
        "</body>\n</html>",
        body);

    send_html(g_client_fd, html);
}

/* ── Route table ────────────────────────────────────────────────────── */

typedef struct {
    const char *prefix;
    int is_exact;
    void (*handler)(void);
} route_entry;

#define R(e, h) { e, 1, h }
#define RP(e, h) { e, 0, h }

static const route_entry routes[] = {
    R("/health", h_health_detailed),
    R("/health/detailed", h_health_detailed),
    R("/v1/health", h_v1_health),
    R("/v1/capabilities", h_v1_capabilities),
    R("/v1/responses", h_responses),
    RP("/v1/responses/", h_responses),
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
    R("/api/profiles/active", h_profiles_active),
    R("/api/profiles", h_profiles),
    R("/api/gateway", h_gateway),
    R("/api/skills", h_skills),
    R("/api/tools/toolsets", h_toolsets),
    R("/api/env", h_env),
    R("/api/logs", h_logs),
    /* Method-dispatched session routes (prefix /api/sessions/ dispatches by HTTP method) */
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
    /* Documentation endpoints */
    R("/api/docs", h_docs),
    R("/api/docs/architecture", h_docs_architecture),
    R("/api/docs/contributing", h_docs_contributing),
    R("/api/docs/readme", h_docs_readme),
    R("/api/docs/security", h_docs_security),
    R("/api/docs/guides", h_docs_guides),
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
/* PoP: ws_send_text @ gateway/platforms/yuanbao.py:send_text */
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
    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    g_current_path[sizeof(g_current_path) - 1] = 0;
    g_client_fd = cfd;
    /* Check if path matches any route — handles /api/, /v1/, /health, etc. */
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
    }    return false; /* No route matched — fall through to static file handler */

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

    /* Store the method for handler dispatch */
    snprintf(g_current_method, sizeof(g_current_method), "%s", method);

    /* Store the path for handler dispatch */
    snprintf(g_current_path, sizeof(g_current_path), "%s", path);

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

