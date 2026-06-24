/**
 * @file api_server.c
 /* E01: OpenAI-compatible REST API server.
  *
  * Minimal HTTP server implementing OpenAI's chat completions + Responses API.
  * Non-streaming only for now. Uses raw sockets (no external HTTP server dep).
  *
  * Endpoints:
  *   GET /health                 → Simple health check
  *   GET /v1/health              → Same as /health (alias)
  *   GET /health/detailed        → Rich health with metrics
  *   GET /v1/models              → JSON list of available model IDs
  *   POST /v1/chat/completions   → OpenAI-compatible chat response
  *   POST /v1/responses          → OpenAI Responses API (GW16)
  *   GET /v1/responses/{id}      → Get stored response (GW16)
  *   DELETE /v1/responses/{id}   → Delete stored response (GW16)
  *   GET /v1/sessions            → List sessions
  *   POST /v1/sessions           → Create session
  *   GET /v1/sessions/{id}       → Get session details
  *   PATCH /v1/sessions/{id}     → Update session metadata (GW16)
  *   DELETE /v1/sessions/{id}    → Delete session
  *   GET /v1/sessions/{id}/messages → Session message history (GW16)
  *   POST /v1/sessions/{id}/fork    → Fork session (GW16)
  *   GET /v1/skills              → List installed skills (GW16)
  *   GET /v1/toolsets            → List toolsets with tools (GW16)
  *   GET /v1/tools               → List available tools (GW16)
  *   GET /v1/agent/status        → Agent live status
  *   GET /v1/config              → Get current config
  *   GET /v1/service/info        → Service metadata
  *   GET /v1/metrics             → Performance counters
  *   GET /v1/capabilities        → Server capabilities
  *   POST /webhook/{platform}    → Inbound webhook relay
  */

/* PoP: REST API server (port of acp_adapter/server) */

#include "hermes_api_server.h"
#include "hermes_json.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

/* ── Constants ──────────────────────────────────────────────────── */

#define API_SERVER_BACKLOG    16
#define API_REQ_BUF_SIZE      65536
#define API_RESP_BUF_SIZE     262144
#define API_DEFAULT_PORT      9101

/* ── Global state ───────────────────────────────────────────────── */

static int              g_port = API_DEFAULT_PORT;
static int              g_server_fd = -1;
static volatile bool    g_running = false;
static pthread_t        g_thread;
static hermes_config_t *g_cfg = NULL;
static agent_state_t   *g_agent = NULL;

/* ── Metrics ────────────────────────────────────────────────────── */

static volatile int     g_request_count = 0;
static volatile time_t  g_start_time = 0;

/* ── HTTP response helpers ──────────────────────────────────────── */

static void send_response(int fd, int status, const char *status_text,
                           const char *body, const char *content_type) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type ? content_type : "application/json",
        body ? strlen(body) : 0);

    write(fd, header, (size_t)n);
    if (body) write(fd, body, strlen(body));
}

static void send_json(int fd, int status, const char *status_text,
                       const char *json_body) {
    send_response(fd, status, status_text, json_body, "application/json");
}

static void send_error(int fd, int status, const char *message) {
    char body[4096];
    snprintf(body, sizeof(body),
        "{\"error\":{\"message\":\"%s\",\"type\":\"error\",\"code\":%d}}",
        message, status);
    send_json(fd, status, message, body);
}

/* ── HTTP request parser (minimal) ──────────────────────────────── */

/**
 * Parse the HTTP method and path from the first line.
 * Returns 0 on success.
 */
static int parse_request_line(const char *buf, char *method, size_t mlen,
                                char *path, size_t plen) {
    if (!buf || !method || !path) return -1;
    method[0] = path[0] = '\0';

    const char *p = buf;

    /* Read method */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && mlen > 1) {
        *method++ = *p++;
        mlen--;
    }
    *method = '\0';
    if (*p == ' ') p++;

    /* Read path */
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && *p != '?' && plen > 1) {
        *path++ = *p++;
        plen--;
    }
    *path = '\0';

    return 0;
}

/**
 * Find the request body (after \r\n\r\n).
 * Returns pointer to body, or NULL if not found.
 */
static const char *find_body(const char *buf) {
    const char *p = strstr(buf, "\r\n\r\n");
    if (!p) {
        p = strstr(buf, "\n\n");
        if (!p) return NULL;
        return p + 2;
    }
    return p + 4;
}

/* ── Route handlers ─────────────────────────────────────────────── */

/**
 * Parse query string from request buffer into a simple key=value map.
 * Extracts the portion after '?' in the request line.
 */
static void parse_query_params(const char *buf, char *query, size_t qlen) {
    query[0] = '\0';
    const char *q = strchr(buf, '?');
    if (!q) return;
    q++; /* skip '?' */
    const char *space = strchr(q, ' ');
    size_t len = space ? (size_t)(space - q) : strlen(q);
    if (len >= qlen) len = qlen - 1;
    memcpy(query, q, len);
    query[len] = '\0';
}

/**
 * Get a query parameter value by name.
 * Returns pointer to value or NULL if not found.
 */
static const char *get_query_param(const char *query, const char *name) {
    if (!query || !query[0] || !name) return NULL;
    size_t nlen = strlen(name);

    const char *p = query;
    while (*p) {
        /* Skip leading '&' if not first */
        if (p != query && *p == '&') p++;

        if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
            return p + nlen + 1;
        }

        /* Skip to next param */
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
    return NULL;
}

/**
 * GET /v1/models — return list of available models.
 */
static void handle_get_models(int fd) {
    /* Build models list from config and provider metadata */
    json_t *models = json_array();

    /* Add known models */
    json_append(models, json_string("hermes-c-default"));
    json_append(models, json_string("gpt-4o"));
    json_append(models, json_string("gpt-4o-mini"));
    json_append(models, json_string("claude-sonnet-4"));
    json_append(models, json_string("claude-haiku-3"));
    json_append(models, json_string("deepseek-chat"));
    json_append(models, json_string("deepseek-reasoner"));

    /* Add model from agent config if available */
    if (g_agent && g_agent->llm.model[0]) {
        json_t *m = json_object();
        json_set(m, "id", json_string(g_agent->llm.model));
        json_set(m, "object", json_string("model"));
        json_set(m, "owned_by", json_string("hermes-c"));
        json_append(models, m);
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_set(root, "data", models);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * POST /v1/chat/completions — run a chat completion.
 */
static void handle_post_chat(int fd, const char *body_json) {
    if (!body_json || !body_json[0]) {
        send_error(fd, 400, "empty request body");
        return;
    }

    json_t *req = json_parse(body_json, NULL);
    if (!req) {
        send_error(fd, 400, "invalid JSON");
        return;
    }

    /* Extract model */
    const char *model_raw = json_get_str(req, "model", "");
    char model[256] = "";
    if (model_raw) strncpy(model, model_raw, sizeof(model) - 1);
    (void)model;

    /* M12: Session context from request */
    const char *session_id_str = json_get_str(req, "session_id", "");

    /* Extract messages array */
    const json_t *messages = json_obj_get(req, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        json_free(req);
        send_error(fd, 400, "missing or empty messages array");
        return;
    }

    /* Build message_t list from JSON messages */
    message_t *msg_list[256];
    int msg_count = 0;

    for (size_t i = 0; i < json_len(messages) && msg_count < 256; i++) {
        const json_t *m = json_get(messages, i);
        if (!m) continue;

        const char *role = json_get_str(m, "role", "user");
        const char *content = json_get_str(m, "content", "");

        message_role_t r = MSG_USER;
        if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
        else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
        else if (strcmp(role, "tool") == 0) r = MSG_TOOL;

        msg_list[msg_count++] = message_new(r, content);
    }

    /* Build response JSON matching OpenAI format */
    json_t *resp = json_object();
    json_set(resp, "id", json_string("chatcmpl-0000000000000"));
    json_set(resp, "object", json_string("chat.completion"));
    json_set(resp, "model", json_string(model));

    /* Timestamp */
    time_t now = time(NULL);
    json_set(resp, "created", json_number((double)now));

    /* Bind session context vars on agent state */
    if (g_agent) {
        session_id_str = json_get_str(req, "session_id", "");
        if (!session_id_str[0]) {
            /* Generate a session ID from the model + timestamp */
            time_t sess_now = time(NULL);
            snprintf(g_agent->session_id, sizeof(g_agent->session_id),
                     "sess_api_%ld", (long)sess_now);
        } else {
            snprintf(g_agent->session_id, sizeof(g_agent->session_id),
                     "%s", session_id_str);
        }
        snprintf(g_agent->platform, sizeof(g_agent->platform), "api_server");
        /* Extract optional user/chat metadata from request */
        const char *user_str = json_get_str(req, "user", "");
        if (user_str[0]) {
            snprintf(g_agent->user_id, sizeof(g_agent->user_id), "%s", user_str);
            snprintf(g_agent->user_name, sizeof(g_agent->user_name), "%s", user_str);
        }
        const char *chat_str = json_get_str(req, "chat_id", "");
        if (chat_str[0])
            snprintf(g_agent->chat_id, sizeof(g_agent->chat_id), "%s", chat_str);
        /* Build composite session key from available IDs */
        snprintf(g_agent->session_key, sizeof(g_agent->session_key),
                 "%s:%s", g_agent->platform, g_agent->session_id);
    }

    /* Choices array */
    json_t *choices = json_array();
    json_t *choice = json_object();
    json_t *message_obj = json_object();

    /* Collect all message content */
    char content_buf[32768] = "";
    for (int i = 0; i < msg_count; i++) {
        if (msg_list[i]->content) {
            size_t cur = strlen(content_buf);
            snprintf(content_buf + cur, sizeof(content_buf) - cur,
                     "%s: %s\n",
                     msg_list[i]->role == MSG_SYSTEM ? "system" :
                     msg_list[i]->role == MSG_ASSISTANT ? "assistant" :
                     msg_list[i]->role == MSG_TOOL ? "tool" : "user",
                     msg_list[i]->content);
        }
        message_free(msg_list[i]);
    }

    json_set(message_obj, "role", json_string("assistant"));

    /* If we have an agent, try a real LLM call */
    char result[32768] = "";
    if (g_agent && g_agent->llm.base_url[0]) {
        /* Rebuild messages for LLM call */
        const message_t *llm_msgs[256];
        int llm_count = 0;
        for (size_t i = 0; i < json_len(messages) && llm_count < 256; i++) {
            const json_t *m = json_get(messages, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "user");
            const char *content = json_get_str(m, "content", "");
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            llm_msgs[llm_count++] = message_new(r, content);
        }

        llm_response_t *resp_llm = llm_chat_completion(
            &g_agent->llm, llm_msgs, (size_t)llm_count, NULL);
        if (resp_llm && resp_llm->content) {
            snprintf(result, sizeof(result), "%s", resp_llm->content);
        }
        llm_response_free(resp_llm);

        /* Free the temporary messages */
        for (int i = 0; i < llm_count; i++)
            message_free((message_t *)llm_msgs[i]);
    } else {
        /* No agent available — echo messages back as a mock response */
        snprintf(result, sizeof(result),
                 "Received %d messages. Model: %s. "
                 "Agent dispatch not available — configure a provider to enable LLM calls.",
                 msg_count, model);
    }

    json_set(message_obj, "content", json_string(result));
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", message_obj);

    /* Finish reason */
    json_t *finish = json_object();
    json_set(finish, "reason", json_string("stop"));
    json_set(choice, "finish_reason", json_string("stop"));

    json_append(choices, choice);
    json_set(resp, "choices", choices);

    /* Usage (minimal) */
    json_t *usage = json_object();
    json_set(usage, "prompt_tokens", json_number(0));
    json_set(usage, "completion_tokens", json_number(0));
    json_set(usage, "total_tokens", json_number(0));
    json_set(resp, "usage", usage);

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }

    json_free(resp);
    json_free(req);
}

static void send_sse_event(int fd, const char *data);
static void send_sse_headers(int fd);
static void sse_send_chunk(int fd, const char *content, int index);

/* ── Path helper ────────────────────────────────────────────────── */

/**
 * Check if path starts with a prefix and extract the suffix.
 * E.g. starts_with("/v1/sessions/abc123", "/v1/sessions/", id, 64) → true, id="abc123"
 */
static bool starts_with(const char *path, const char *prefix, char *suffix, size_t slen) {
    size_t plen = strlen(prefix);
    if (strncmp(path, prefix, plen) != 0) return false;
    const char *rest = path + plen;
    if (!rest[0]) return false; /* path = prefix, no suffix */
    strncpy(suffix, rest, slen - 1);
    suffix[slen - 1] = '\0';
    return true;
}

/**
 * GET /v1/sessions — list all sessions.
 */
static void handle_sessions_list(int fd, const char *query) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    size_t count = 0;
    db_session_entry_t *entries = db_list_with_meta(g_agent->db, &count);
    if (!entries) {
        send_json(fd, 200, "OK", "{\"object\":\"list\",\"data\":[],\"total\":0}");
        return;
    }

    /* Apply ?limit=N query param */
    size_t limit = count;
    if (query && query[0]) {
        const char *limit_str = get_query_param(query, "limit");
        if (limit_str) {
            long n = atol(limit_str);
            if (n > 0 && (size_t)n < limit) limit = (size_t)n;
        }
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    json_t *data = json_array();
    for (size_t i = 0; i < limit; i++) {
        json_t *s = json_object();
        json_set(s, "id", json_string(entries[i].id));
        if (entries[i].meta.title[0])
            json_set(s, "title", json_string(entries[i].meta.title));
        if (entries[i].meta.created_at > 0)
            json_set(s, "created_at", json_number((double)entries[i].meta.created_at));
        if (entries[i].meta.updated_at > 0)
            json_set(s, "updated_at", json_number((double)entries[i].meta.updated_at));
        json_set(s, "message_count", json_number((double)entries[i].meta.message_count));
        json_append(data, s);
    }
    json_set(root, "data", data);
    json_set(root, "total", json_number((double)count));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    }
    json_free(root);
    free(entries);
}

/**
 * GET /v1/sessions/{id} — get session details.
 */
static void handle_session_get(int fd, const char *session_id) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    char *json_out = db_export_json(g_agent->db, session_id);
    if (!json_out) {
        send_error(fd, 404, "session not found");
        return;
    }

    send_json(fd, 200, "OK", json_out);
    free(json_out);
}

/**
 * POST /v1/sessions — create a new session.
 */
static void handle_session_create(int fd, const char *body_json) {
    (void)body_json;
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    /* Generate a session ID */
    char session_id[64];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(session_id, sizeof(session_id), "%04d%02d%02d_%02d%02d%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Save empty session data */
    if (!db_save(g_agent->db, session_id, "[]")) {
        send_error(fd, 500, "failed to create session");
        return;
    }

    /* Create and save default metadata */
    session_meta_t meta;
    db_meta_init(&meta);
    snprintf(meta.title, sizeof(meta.title), "%s", session_id);
    if (!db_save_meta(g_agent->db, session_id, &meta)) {
        send_error(fd, 500, "failed to save session metadata");
        return;
    }

    json_t *resp = json_object();
    json_set(resp, "id", json_string(session_id));
    json_set(resp, "object", json_string("session.created"));

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 201, "Created", out);
        free(out);
    }
    json_free(resp);
}

/**
 * DELETE /v1/sessions/{id} — delete a session.
 */
static void handle_session_delete(int fd, const char *session_id) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    if (!db_delete(g_agent->db, session_id)) {
        send_error(fd, 404, "session not found or delete failed");
        return;
    }

    /* Build response */
    char resp[256];
    snprintf(resp, sizeof(resp), "{\"status\":\"deleted\",\"id\":\"%s\"}", session_id);
    send_json(fd, 200, "OK", resp);
}

/* GW16: PATCH /v1/sessions/{id} — update session metadata (title, end_reason) */
static void handle_session_patch(int fd, const char *session_id, const char *body_json) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    /* Load existing metadata */
    session_meta_t meta;
    if (!db_load_meta(g_agent->db, session_id, &meta)) {
        send_error(fd, 404, "session not found");
        return;
    }

    if (!body_json || !*body_json) {
        send_error(fd, 400, "empty request body");
        return;
    }

    json_t *body = json_parse(body_json, NULL);
    if (!body || body->type != JSON_OBJECT) {
        send_error(fd, 400, "invalid JSON body");
        json_free(body);
        return;
    }

    /* Update allowed fields: title, end_reason */
    json_t *title_val = json_obj_get(body, "title");
    json_t *end_reason_val = json_obj_get(body, "end_reason");

    if (title_val && title_val->type == JSON_STRING) {
        snprintf(meta.title, sizeof(meta.title), "%s", title_val->str_val);
    } else if (title_val) {
        json_free(body);
        send_error(fd, 400, "\"title\" must be a string or null");
        return;
    }

    if (end_reason_val && end_reason_val->type == JSON_STRING) {
        snprintf(meta.end_reason, sizeof(meta.end_reason), "%s", end_reason_val->str_val);
        meta.ended_at = time(NULL);
    } else if (end_reason_val) {
        json_free(body);
        send_error(fd, 400, "\"end_reason\" must be a string");
        return;
    }

    meta.updated_at = time(NULL);

    /* Save updated metadata */
    if (!db_save_meta(g_agent->db, session_id, &meta)) {
        json_free(body);
        send_error(fd, 500, "failed to save session metadata");
        return;
    }

    json_free(body);

    /* Build response */
    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.session"));
    json_set(resp, "session_id", json_string(session_id));
    json_set(resp, "title", json_string(meta.title));
    json_set(resp, "end_reason", json_string(meta.end_reason[0] ? meta.end_reason : ""));
    json_set(resp, "ended_at", json_number((double)meta.ended_at));
    json_set(resp, "updated_at", json_number((double)meta.updated_at));

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    }
    json_free(resp);
}

/* GW16: Session messages — GET /v1/sessions/{id}/messages */
static void handle_session_messages(int fd, const char *session_id) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    char *json_data = db_load(g_agent->db, session_id, NULL);
    if (!json_data) {
        send_error(fd, 404, "session not found");
        return;
    }

    /* Parse messages from JSON and return as OpenAI-style list */
    json_t *messages = json_parse(json_data, NULL);
    free(json_data);

    if (!messages) {
        send_error(fd, 500, "failed to parse session data");
        return;
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_set(root, "data", messages);
    json_set(root, "session_id", json_string(session_id));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    }
    json_free(root);
}

/* GW16: Session fork — POST /v1/sessions/{id}/fork */
static void handle_session_fork(int fd, const char *session_id,
                                 const char *body_json) {
    if (!g_agent || !g_agent->db) {
        send_error(fd, 503, "database not initialized");
        return;
    }

    /* Parse optional branch_point from body */
    int branch_point = -1;
    if (body_json && body_json[0]) {
        json_t *body = json_parse(body_json, NULL);
        if (body) {
            json_t *bp = json_obj_get(body, "branch_point");
            if (bp && bp->type == JSON_NUMBER) {
                branch_point = (int)bp->num_val;
            }
            json_free(body);
        }
    }

    /* Generate new session ID */
    char new_id[64];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    snprintf(new_id, sizeof(new_id), "%04d%02d%02d_%02d%02d%02d",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    /* Use db_branch if branch_point specified, otherwise copy all */
    char *forked_data = NULL;
    if (branch_point >= 0) {
        forked_data = db_branch(g_agent->db, session_id, new_id, branch_point);
    } else {
        /* Copy entire session */
        char *src = db_load(g_agent->db, session_id, NULL);
        if (src) {
            db_save(g_agent->db, new_id, src);
            forked_data = db_load(g_agent->db, new_id, NULL);
            free(src);
        }
    }

    if (!forked_data) {
        send_error(fd, 404, "session not found or fork failed");
        return;
    }

    /* Copy metadata with parent link */
    session_meta_t meta;
    if (db_load_meta(g_agent->db, session_id, &meta)) {
        db_meta_init(&meta);
        snprintf(meta.parent_id, sizeof(meta.parent_id), "%s", session_id);
        meta.branch_point = branch_point;
        snprintf(meta.title, sizeof(meta.title), "%s", new_id);
        db_save_meta(g_agent->db, new_id, &meta);
    }

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"id\":\"%s\",\"parent_id\":\"%s\",\"branch_point\":%d}",
             new_id, session_id, branch_point);
    send_json(fd, 201, "Created", resp);
    free(forked_data);
}

/* GW16: Responses API — POST /v1/responses */
static void handle_post_responses(int fd, const char *body_json) {
    if (!body_json || !body_json[0]) {
        send_error(fd, 400, "empty request body");
        return;
    }

    /* Parse the Responses API request */
    json_t *req = json_parse(body_json, NULL);
    if (!req) {
        send_error(fd, 400, "invalid JSON");
        return;
    }

    /* Extract model, input, and previous_response_id */
    const char *model = "";
    json_t *model_node = json_obj_get(req, "model");
    if (model_node && model_node->type == JSON_STRING)
        model = model_node->str_val;

    json_t *input = json_obj_get(req, "input");
    const char *previous_response_id = "";
    json_t *prev_id_node = json_obj_get(req, "previous_response_id");
    if (prev_id_node && prev_id_node->type == JSON_STRING)
        previous_response_id = prev_id_node->str_val;

    /* Build messages from input (supports string or array) */
    json_t *messages = json_array();
    if (input) {
        if (input->type == JSON_STRING) {
            json_t *msg = json_object();
            json_set(msg, "role", json_string("user"));
            json_set(msg, "content", json_string(input->str_val));
            json_append(messages, msg);
        } else if (input->type == JSON_ARRAY) {
            size_t len = json_len(input);
            for (size_t i = 0; i < len; i++) {
                json_t *item = json_get(input, i);
                if (item) json_append(messages, item);
            }
        }
    }

    /* If previous_response_id, load stored conversation */
    if (previous_response_id[0]) {
        /* For now, just acknowledge it — full implementation would
         * load the previous response and continue the conversation */
    }

    /* M12: Bind session context vars on agent state */
    if (g_agent) {
        const char *sid = json_get_str(req, "session_id", "");
        if (sid[0])
            snprintf(g_agent->session_id, sizeof(g_agent->session_id), "%s", sid);
        snprintf(g_agent->platform, sizeof(g_agent->platform), "api_server");
        snprintf(g_agent->session_key, sizeof(g_agent->session_key),
                 "%s:%s", g_agent->platform, g_agent->session_id);
    }

    /* Dispatch through LLM if available */
    char *response_text = NULL;
    if (g_agent && g_agent->llm.base_url[0]) {
        size_t msg_count = json_len(messages);
        if (msg_count > 0) {
            const message_t **llm_msgs = malloc(msg_count * sizeof(message_t *));
            if (llm_msgs) {
                for (size_t i = 0; i < msg_count; i++) {
                    json_t *m = (json_t *)json_get(messages, i);
                    if (!m) continue;
                    const char *role = json_get_str(m, "role", "user");
                    const char *content = json_get_str(m, "content", "");
                    message_role_t r = MSG_USER;
                    if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
                    else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
                    llm_msgs[i] = message_new(r, content);
                }
                llm_response_t *resp_llm = llm_chat_completion(
                    &g_agent->llm, llm_msgs, msg_count, NULL);
                if (resp_llm && resp_llm->content) {
                    response_text = strdup(resp_llm->content);
                }
                llm_response_free(resp_llm);
                for (size_t i = 0; i < msg_count; i++)
                    message_free((message_t *)llm_msgs[i]);
                free(llm_msgs);
            }
        }
    }
    if (!response_text) {
        /* No agent — echo back */
        response_text = strdup("Responses API received input — "
            "configure a provider to enable LLM calls.");
    }

    /* Build Responses API response */
    json_t *resp = json_object();
    char response_id[64];
    snprintf(response_id, sizeof(response_id), "resp_%ld", (long)time(NULL));
    json_set(resp, "id", json_string(response_id));
    json_set(resp, "object", json_string("response"));
    json_set(resp, "created", json_number((double)time(NULL)));
    if (model[0])
        json_set(resp, "model", json_string(model));

    json_t *output = json_array();
    if (response_text && response_text[0]) {
        json_t *content_item = json_object();
        json_set(content_item, "type", json_string("message"));
        json_set(content_item, "role", json_string("assistant"));
        json_t *content_arr = json_array();
        json_t *text_item = json_object();
        json_set(text_item, "type", json_string("output_text"));
        json_set(text_item, "text", json_string(response_text));
        json_append(content_arr, text_item);
        json_set(content_item, "content", content_arr);
        json_append(output, content_item);
    }
    json_set(resp, "output", output);

    char *out = json_serialize(resp);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    }
    json_free(resp);
    json_free(req);
    json_free(messages);
    if (response_text) free(response_text);
}

/* GW16: Get response — GET /v1/responses/{response_id} */
static void handle_get_response(int fd, const char *response_id) {
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "resp_%s", response_id);

    /* Try loading from DB if available */
    if (g_agent && g_agent->db) {
        char *data = db_load(g_agent->db, buf, NULL);
        if (data) {
            send_json(fd, 200, "OK", data);
            free(data);
            return;
        }
    }

    /* Fallback stub */
    n = snprintf(buf, sizeof(buf),
             "{\"id\":\"%s\",\"object\":\"response\",\"status\":\"completed\"}",
             response_id);
    send_json(fd, 200, "OK", buf);
    (void)n;
}

/* GW16: Delete response — DELETE /v1/responses/{response_id} */
static void handle_delete_response(int fd, const char *response_id) {
    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"id\":\"%s\",\"object\":\"response\",\"deleted\":true}",
             response_id);
    send_json(fd, 200, "OK", resp);
}

/**
 * POST /v1/chat/completions?stream=true — SSE streaming response.
 * Sends the complete response as token-by-token SSE events.
 * The handler closes the fd when done.
 */
static void handle_post_chat_stream(int fd, const char *body_json) {
    if (!body_json || !body_json[0]) {
        send_error(fd, 400, "empty request body");
        close(fd);
        return;
    }

    json_t *req = json_parse(body_json, NULL);
    if (!req) {
        send_error(fd, 400, "invalid JSON");
        close(fd);
        return;
    }

    /* Extract model and messages */
    const char *model_raw = json_get_str(req, "model", "");
    char model[256] = "";
    if (model_raw) strncpy(model, model_raw, sizeof(model) - 1);
    const json_t *messages = json_obj_get(req, "messages");
    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0) {
        json_free(req);
        send_error(fd, 400, "missing or empty messages array");
        close(fd);
        return;
    }

    /* Run through LLM if available */
    char result[32768] = "";
    if (g_agent && g_agent->llm.base_url[0]) {
        const message_t *llm_msgs[256];
        int llm_count = 0;
        for (size_t i = 0; i < json_len(messages) && llm_count < 256; i++) {
            const json_t *m = json_get(messages, i);
            if (!m) continue;
            const char *role = json_get_str(m, "role", "user");
            const char *content = json_get_str(m, "content", "");
            message_role_t r = MSG_USER;
            if (strcmp(role, "system") == 0) r = MSG_SYSTEM;
            else if (strcmp(role, "assistant") == 0) r = MSG_ASSISTANT;
            llm_msgs[llm_count++] = message_new(r, content);
        }

        llm_response_t *resp_llm = llm_chat_completion(
            &g_agent->llm, llm_msgs, (size_t)llm_count, NULL);
        if (resp_llm && resp_llm->content) {
            snprintf(result, sizeof(result), "%s", resp_llm->content);
        }
        llm_response_free(resp_llm);
        for (int i = 0; i < llm_count; i++)
            message_free((message_t *)llm_msgs[i]);
    }

    json_free(req);

    /* Send SSE headers and stream the response */
    send_sse_headers(fd);

    /* Token-buffer SSE streaming — send in ~4-char chunks for natural
     * token-level granularity, respecting UTF-8 multi-byte boundaries. */
#define TOKEN_BUF_SIZE 4
    char tbuf[TOKEN_BUF_SIZE + 1];
    int tpos = 0;
    int index = 0;
    int i = 0;

    while (result[i]) {
        /* Track multi-byte UTF-8: continuation bytes are 0x80-0xBF */
        int is_utf8_cont = ((unsigned char)result[i] & 0xC0) == 0x80;
        tbuf[tpos++] = result[i++];

        if (!is_utf8_cont && tpos >= TOKEN_BUF_SIZE) {
            tbuf[tpos] = '\0';
            sse_send_chunk(fd, tbuf, index++);
            tpos = 0;
        }
    }

    /* Flush remaining chars */
    if (tpos > 0) {
        tbuf[tpos] = '\0';
        sse_send_chunk(fd, tbuf, index++);
    }

    /* Fallback: if no chunks, send full result as one chunk */
    if (index == 0 && result[0]) {
        sse_send_chunk(fd, result, 0);
        index = 1;
    }

    /* Send finish event */
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_t *delta = json_object();
    json_set(delta, "role", json_string("assistant"));
    json_set(delta, "content", json_string(""));
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", json_string("stop"));

    json_t *finish_msg = json_object();
    json_set(finish_msg, "id", json_string("chatcmpl-stream"));
    json_set(finish_msg, "object", json_string("chat.completion.chunk"));
    json_set(finish_msg, "created", json_number((double)time(NULL)));
    json_set(finish_msg, "model", json_string(model));
    json_t *choices_arr = json_array();
    json_append(choices_arr, choice);
    json_set(finish_msg, "choices", choices_arr);

    char *finish_str = json_serialize(finish_msg);
    if (finish_str) {
        send_sse_event(fd, finish_str);
        free(finish_str);
    }
    json_free(finish_msg);

    /* End of stream */
    send_sse_event(fd, "[DONE]");
    fsync(fd);
    close(fd);
}

/**
 * Handle an OPTIONS preflight request.
 */
static void handle_options(int fd) {
    send_response(fd, 204, "No Content", NULL, NULL);
}

/**
 * GET /v1/capabilities — machine-readable API capabilities.
 */
static void handle_capabilities(int fd) {
    json_t *root = json_object();
    json_set(root, "api_version", json_string("v1"));
    json_set(root, "name", json_string("hermes-c"));

    json_t *endpoints = json_array();
    json_append(endpoints, json_string("GET /v1/models"));
    json_append(endpoints, json_string("POST /v1/chat/completions"));
    json_append(endpoints, json_string("GET /v1/capabilities"));
    json_append(endpoints, json_string("GET /v1/tools"));
    json_append(endpoints, json_string("GET /v1/agent/status"));
    json_append(endpoints, json_string("GET /v1/sessions"));
    json_append(endpoints, json_string("POST /v1/sessions"));
    json_append(endpoints, json_string("GET /v1/sessions/{id}"));
    json_append(endpoints, json_string("DELETE /v1/sessions/{id}"));
    json_append(endpoints, json_string("GET /health"));
    json_append(endpoints, json_string("GET /health/detailed"));
    json_set(root, "endpoints", endpoints);

    json_t *features = json_object();
    json_set(features, "chat_completions", json_bool(true));
    json_set(features, "streaming", json_bool(true));
    json_set(features, "tools_listing", json_bool(true));
    json_set(features, "models_listing", json_bool(true));
    json_set(features, "agent_status", json_bool(true));
    json_set(features, "sessions_listing", json_bool(true));
    json_set(features, "sessions_crud", json_bool(true));
    json_set(features, "health_detailed", json_bool(true));
    json_set(root, "features", features);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/tools — list all registered tools.
 */
static void handle_tools_list(int fd) {
    if (!g_agent) {
        send_error(fd, 503, "agent not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    json_t *tools_array = json_array();
    size_t tool_count = registry_get_count();

    for (size_t i = 0; i < tool_count; i++) {
        const char *name = registry_get_name(i);
        if (!name) continue;

        json_t *tool_obj = json_object();
        json_set(tool_obj, "name", json_string(name));

        const char *toolset = registry_get_toolset(name);
        if (toolset) json_set(tool_obj, "toolset", json_string(toolset));

        int timeout = registry_get_timeout(name);
        if (timeout > 0) json_set(tool_obj, "timeout", json_number((double)timeout));

        json_append(tools_array, tool_obj);
    }

    json_set(root, "data", tools_array);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* GW16: GET /v1/skills — list installed skills */
static void handle_skills(int fd) {
    json_t *root = json_object();
    json_set(root, "object", json_string("list"));

    skill_list_t *list = skills_scan_all();
    if (list) {
        json_t *data = json_array();
        for (size_t i = 0; i < list->count; i++) {
            json_t *entry = json_object();
            json_set(entry, "name", json_string(list->skills[i].name));
            if (list->skills[i].description[0])
                json_set(entry, "description", json_string(list->skills[i].description));
            json_append(data, entry);
        }
        json_set(root, "data", data);
        json_set(root, "count", json_number((double)list->count));
        skills_scan_free(list);
    } else {
        json_set(root, "data", json_array());
        json_set(root, "count", json_number(0));
    }

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* GW16: GET /v1/toolsets — list toolsets with their tools */
static void handle_toolsets(int fd) {
    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_t *data = json_array();

    /* Collect unique toolsets and their tools by scanning all registered tools */
    size_t tool_count = registry_get_count();
    if (tool_count == 0) {
        json_set(root, "data", data);
        char *out = json_serialize(root);
        if (out) { send_json(fd, 200, "OK", out); free(out); }
        else { send_error(fd, 500, "serialization failed"); }
        json_free(root);
        return;
    }

    /* First pass: collect unique toolset names */
    typedef struct { char name[32]; } tset_entry_t;
    tset_entry_t toolsets[256];
    size_t tset_count = 0;
    for (size_t i = 0; i < tool_count; i++) {
        const char *name = registry_get_name(i);
        if (!name) continue;
        const char *ts = registry_get_toolset(name);
        if (!ts || !ts[0]) continue;

        bool found = false;
        for (size_t j = 0; j < tset_count; j++) {
            if (strcmp(toolsets[j].name, ts) == 0) { found = true; break; }
        }
        if (!found && tset_count < 256) {
            snprintf(toolsets[tset_count].name, sizeof(toolsets[tset_count].name), "%s", ts);
            tset_count++;
        }
    }

    /* Second pass: add tools to each toolset */
    for (size_t t = 0; t < tset_count; t++) {
        json_t *tset_obj = json_object();
        json_set(tset_obj, "toolset", json_string(toolsets[t].name));
        json_t *tools_arr = json_array();
        for (size_t i = 0; i < tool_count; i++) {
            const char *name = registry_get_name(i);
            if (!name) continue;
            const char *ts = registry_get_toolset(name);
            if (ts && strcmp(ts, toolsets[t].name) == 0) {
                json_append(tools_arr, json_string(name));
            }
        }
        json_set(tset_obj, "tools", tools_arr);
        json_append(data, tset_obj);
    }

    json_set(root, "data", data);
    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /health/detailed — rich health status with system info.
 */
static void handle_health_detailed(int fd) {
    json_t *root = json_object();

    /* Basic status */
    json_set(root, "status", json_string("ok"));
    json_set(root, "version", json_string(HERMES_VERSION));

    /* Uptime */
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    json_set(root, "uptime_seconds", json_number((double)(time(NULL) - start_time)));

    /* Agent state */
    if (g_agent) {
        json_t *agent_info = json_object();
        if (g_agent->llm.model[0])
            json_set(agent_info, "model", json_string(g_agent->llm.model));
        if (g_agent->llm.provider[0])
            json_set(agent_info, "provider", json_string(g_agent->llm.provider));
        json_set(agent_info, "tools_registered", json_number((double)registry_get_count()));
        json_set(agent_info, "session_active", json_bool(g_agent->db != NULL));
        json_set(root, "agent", agent_info);
    }

    /* Config port */
    json_set(root, "port", json_number((double)g_port));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/agent/status — current agent state information.
 */
static void handle_agent_status(int fd) {
    if (!g_agent) {
        send_error(fd, 503, "agent not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "status", json_string("running"));

    /* LLM configuration */
    json_t *llm_info = json_object();
    if (g_agent->llm.model[0])
        json_set(llm_info, "model", json_string(g_agent->llm.model));
    if (g_agent->llm.provider[0])
        json_set(llm_info, "provider", json_string(g_agent->llm.provider));
    if (g_agent->llm.base_url[0])
        json_set(llm_info, "base_url", json_string(g_agent->llm.base_url));
    json_set(llm_info, "max_tokens", json_number((double)g_agent->llm.max_tokens));
    json_set(llm_info, "temperature", json_number((double)g_agent->llm.temperature));
    json_set(root, "llm", llm_info);

    /* Session info */
    json_t *session_info = json_object();
    json_set(session_info, "active", json_bool(g_agent->db != NULL));
    if (g_agent->session_id[0])
        json_set(session_info, "session_id", json_string(g_agent->session_id));
    json_set(root, "session", session_info);

    /* Config */
    json_set(root, "max_iterations", json_number((double)g_agent->max_iterations));

    /* Tool info */
    json_set(root, "tools_registered", json_number((double)registry_get_count()));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* ── Webhook endpoint (E05) ─────────────────────────────────────── */

/* Handle POST /webhook/:platform — accepts generic webhook payloads.
 * Logs the incoming webhook and returns acknowledgment. */
static void handle_webhook(int fd, const char *body, const char *platform) {
    if (!platform || !platform[0]) {
        send_error(fd, 400, "missing platform");
        close(fd);
        return;
    }
    if (!body || !body[0]) {
        send_error(fd, 400, "empty body");
        close(fd);
        return;
    }

    json_t *resp = json_object();
    json_set(resp, "status", json_string("ok"));
    json_set(resp, "platform", json_string(platform));
    json_set(resp, "received", json_bool(true));
    json_set(resp, "body_size", json_number((double)strlen(body)));

    /* Log */
    fprintf(stderr, "[webhook] POST /webhook/%s — %zu bytes\n",
            platform, body ? strlen(body) : 0);

    char *s = json_serialize(resp);
    send_json(fd, 200, "OK", s ? s : "{}");
    free(s);
    json_free(resp);
}

/* ── E01: Remaining REST endpoints ──────────────────────────────── */

/**
 * GET /v1/config — expose safe configuration fields (no secrets).
 */
static void handle_config_get(int fd) {
    if (!g_cfg) {
        send_error(fd, 503, "config not initialized");
        return;
    }

    json_t *root = json_object();
    json_set(root, "version", json_string(HERMES_VERSION));
    json_set(root, "port", json_number((double)g_port));

    /* LLM config */
    json_t *llm_cfg = json_object();
    json_set(llm_cfg, "model", json_string(g_cfg->model));
    json_set(llm_cfg, "provider", json_string(g_cfg->provider));
    json_set(llm_cfg, "base_url", json_string(g_cfg->base_url));
    if (g_agent) {
        json_set(llm_cfg, "max_tokens", json_number((double)g_agent->llm.max_tokens));
        json_set(llm_cfg, "temperature", json_number((double)g_agent->llm.temperature));
    }
    json_set(llm_cfg, "max_iterations", json_number((double)g_cfg->agent.max_iterations));
    json_set(root, "llm", llm_cfg);

    /* Agent config (safe fields only) */
    json_t *agent_cfg = json_object();
    json_set(agent_cfg, "verbose", json_number((double)g_cfg->verbose));
    json_set(agent_cfg, "quiet_mode", json_bool(g_cfg->quiet_mode));
    json_set(agent_cfg, "yolo_mode", json_bool(g_cfg->yolo_mode));
    json_set(agent_cfg, "fast_mode", json_bool(g_cfg->fast_mode));
    json_set(agent_cfg, "compress_enabled", json_bool(g_cfg->compress_enabled));
    json_set(agent_cfg, "max_turns", json_number((double)g_cfg->max_turns));
    json_set(agent_cfg, "personality", json_string(g_cfg->personality[0] ? g_cfg->personality : "default"));
    json_set(root, "agent", agent_cfg);

    /* Gateway */
    json_t *gw = json_object();
    json_set(gw, "platforms", json_string(g_cfg->gateway_platforms[0] ? g_cfg->gateway_platforms : "none"));
    json_set(gw, "secret_rotation", json_number((double)g_cfg->secret_rotation_interval));
    json_set(root, "gateway", gw);

    /* Home / paths */
    json_t *paths = json_object();
    json_set(paths, "vault_path", json_string(g_cfg->vault_path[0] ? g_cfg->vault_path : "default"));
    json_set(paths, "skin", json_string(g_cfg->skin_path[0] ? g_cfg->skin_path : "default"));
    json_set(root, "paths", paths);

    /* Proxy */
    json_t *proxy = json_object();
    if (g_cfg->proxy_https[0]) json_set(proxy, "https_proxy", json_string(g_cfg->proxy_https));
    if (g_cfg->proxy_no[0]) json_set(proxy, "no_proxy", json_string(g_cfg->proxy_no));
    json_set(root, "proxy", proxy);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/service/info — service metadata (version, uptime, build).
 */
static void handle_service_info(int fd) {
    json_t *root = json_object();
    json_set(root, "service", json_string("hermes-c"));
    json_set(root, "version", json_string(HERMES_VERSION));
    json_set(root, "api_version", json_string("v1"));

    if (g_start_time > 0) {
        time_t now = time(NULL);
        double uptime = difftime(now, g_start_time);
        json_set(root, "uptime_seconds", json_number(uptime));
        json_set(root, "start_time", json_number((double)g_start_time));
    }

    json_t *build = json_object();
    json_set(build, "compiler", json_string(__VERSION__));
#if defined(__linux__)
    json_set(build, "platform", json_string("linux"));
#elif defined(__APPLE__)
    json_set(build, "platform", json_string("darwin"));
#elif defined(_WIN32)
    json_set(build, "platform", json_string("windows"));
#else
    json_set(build, "platform", json_string("unknown"));
#endif
    json_set(root, "build", build);

    /* Registered endpoints summary */
    json_t *endpoints = json_array();
    json_append(endpoints, json_string("GET /v1/models"));
    json_append(endpoints, json_string("POST /v1/chat/completions"));
    json_append(endpoints, json_string("GET /v1/capabilities"));
    json_append(endpoints, json_string("GET /v1/tools"));
    json_append(endpoints, json_string("GET /v1/agent/status"));
    json_append(endpoints, json_string("GET /v1/sessions"));
    json_append(endpoints, json_string("POST /v1/sessions"));
    json_append(endpoints, json_string("GET /v1/sessions/{id}"));
    json_append(endpoints, json_string("DELETE /v1/sessions/{id}"));
    json_append(endpoints, json_string("GET /health"));
    json_append(endpoints, json_string("GET /health/detailed"));
    json_append(endpoints, json_string("GET /v1/config"));
    json_append(endpoints, json_string("GET /v1/service/info"));
    json_append(endpoints, json_string("GET /v1/metrics"));
    json_append(endpoints, json_string("POST /webhook/:platform"));
    json_set(root, "endpoints", endpoints);

    json_set(root, "client_count", json_number(0));
    json_set(root, "request_count", json_number((double)g_request_count));

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/**
 * GET /v1/metrics — request and token usage metrics.
 */
static void handle_metrics_get(int fd) {
    json_t *root = json_object();
    json_set(root, "object", json_string("metrics"));

    json_set(root, "total_requests", json_number((double)g_request_count));
    json_set(root, "uptime_seconds", json_number(
        g_start_time > 0 ? difftime(time(NULL), g_start_time) : 0));

    /* Per-endpoint request types (basic tracking) */
    json_t *endpoints = json_object();
    /* Simple counter per method+path — tracked inline in dispatch */
    json_set(endpoints, "chat_completions", json_number(0));
    json_set(endpoints, "models", json_number(0));
    json_set(endpoints, "sessions", json_number(0));
    json_set(root, "endpoints", endpoints);

    char *out = json_serialize(root);
    if (out) {
        send_json(fd, 200, "OK", out);
        free(out);
    } else {
        send_error(fd, 500, "serialization failed");
    }
    json_free(root);
}

/* ── Request dispatch ───────────────────────────────────────────── */

static void dispatch_request(int client_fd, const char *method,
                              const char *path, const char *body,
                              const char *query) {
    /* Track request count */
    __atomic_add_fetch(&g_request_count, 1, __ATOMIC_SEQ_CST);

    if (strcmp(method, "OPTIONS") == 0) {
        handle_options(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/models") == 0) {
        handle_get_models(client_fd);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/chat/completions") == 0 && strstr(query ? query : "", "stream=true")) {
        handle_post_chat_stream(client_fd, body);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/chat/completions") == 0) {
        handle_post_chat(client_fd, body);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/sessions") == 0) {
        handle_sessions_list(client_fd, query);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/sessions") == 0) {
        handle_session_create(client_fd, body);
    } else if (strcmp(method, "GET") == 0 && strncmp(path, "/v1/sessions/", 13) == 0) {
        char sid[64] = "";
        if (starts_with(path, "/v1/sessions/", sid, sizeof(sid)))
            handle_session_get(client_fd, sid);
        else
            send_error(client_fd, 400, "invalid session id");
    } else if (strcmp(method, "DELETE") == 0 && strncmp(path, "/v1/sessions/", 13) == 0) {
        char sid[64] = "";
        if (starts_with(path, "/v1/sessions/", sid, sizeof(sid)))
            handle_session_delete(client_fd, sid);
        else
            send_error(client_fd, 400, "invalid session id");
    } else if (strcmp(method, "PATCH") == 0 && strncmp(path, "/v1/sessions/", 13) == 0) {
        /* GW16: PATCH /v1/sessions/{id} — update session metadata */
        char sid[64] = "";
        if (starts_with(path, "/v1/sessions/", sid, sizeof(sid)))
            handle_session_patch(client_fd, sid, body);
        else
            send_error(client_fd, 400, "invalid session id");
    } else if (strcmp(method, "GET") == 0 && strncmp(path, "/v1/sessions/", 13) == 0 && strstr(path + 13, "/messages")) {
        /* GW16: GET /v1/sessions/{id}/messages */
        char sid[64] = "";
        const char *p = path + 13; /* skip /v1/sessions/ */
        size_t len = strcspn(p, "/");
        if (len > 0 && len < sizeof(sid)) {
            memcpy(sid, p, len);
            sid[len] = '\0';
            handle_session_messages(client_fd, sid);
        } else {
            send_error(client_fd, 400, "invalid session id");
        }
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/v1/sessions/", 13) == 0 && strstr(path + 13, "/fork")) {
        /* GW16: POST /v1/sessions/{id}/fork */
        char sid[64] = "";
        const char *p = path + 13; /* skip /v1/sessions/ */
        size_t len = strcspn(p, "/");
        if (len > 0 && len < sizeof(sid)) {
            memcpy(sid, p, len);
            sid[len] = '\0';
            handle_session_fork(client_fd, sid, body);
        } else {
            send_error(client_fd, 400, "invalid session id");
        }
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/v1/responses") == 0) {
        /* GW16: POST /v1/responses */
        handle_post_responses(client_fd, body);
    } else if (strcmp(method, "GET") == 0 && strncmp(path, "/v1/responses/", 15) == 0) {
        /* GW16: GET /v1/responses/{response_id} */
        const char *response_id = path + 15;
        if (response_id[0])
            handle_get_response(client_fd, response_id);
        else
            send_error(client_fd, 400, "invalid response id");
    } else if (strcmp(method, "DELETE") == 0 && strncmp(path, "/v1/responses/", 15) == 0) {
        /* GW16: DELETE /v1/responses/{response_id} */
        const char *response_id = path + 15;
        if (response_id[0])
            handle_delete_response(client_fd, response_id);
        else
            send_error(client_fd, 400, "invalid response id");
    } else if (strcmp(method, "GET") == 0 && (strcmp(path, "/health") == 0 || strcmp(path, "/v1/health") == 0)) {
        send_json(client_fd, 200, "OK", "{\"status\":\"ok\"}");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/health/detailed") == 0) {
        handle_health_detailed(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/capabilities") == 0) {
        handle_capabilities(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/tools") == 0) {
        handle_tools_list(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/skills") == 0) {
        /* GW16: GET /v1/skills — list installed skills */
        handle_skills(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/toolsets") == 0) {
        /* GW16: GET /v1/toolsets — list toolsets with their tools */
        handle_toolsets(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/agent/status") == 0) {
        handle_agent_status(client_fd);
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/webhook/", 9) == 0) {
        /* Extract platform from path: /webhook/telegram, /webhook/discord, etc. */
        const char *platform = path + 9;
        handle_webhook(client_fd, body, platform);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/config") == 0) {
        handle_config_get(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/service/info") == 0) {
        handle_service_info(client_fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/v1/metrics") == 0) {
        handle_metrics_get(client_fd);
    } else {
        send_error(client_fd, 404, "not found");
    }
}

/* ── Connection handler ─────────────────────────────────────────── */

static void handle_client(int client_fd) {
    char buf[API_REQ_BUF_SIZE];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';

    char method[16] = "", path[1024] = "";
    if (parse_request_line(buf, method, sizeof(method), path, sizeof(path)) != 0) {
        send_error(client_fd, 400, "bad request");
        close(client_fd);
        return;
    }

    /* Extract query string from request line (path portion after ?) */
    char query[2048] = "";
    parse_query_params(buf, query, sizeof(query));

    const char *body = find_body(buf);
    dispatch_request(client_fd, method, path, body, query);
    close(client_fd);
}

/* ── SSE helpers ────────────────────────────────────────────────── */

static void send_sse_event(int fd, const char *data) {
    dprintf(fd, "data: %s\n\n", data ? data : "");
}

static void send_sse_headers(int fd) {
    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    write(fd, headers, strlen(headers));
}

static void sse_send_chunk(int fd, const char *content, int index) {
    char event[4096];
    snprintf(event, sizeof(event),
        "{\"choices\":[{\"delta\":{\"content\":\"%s\"},\"index\":%d}]}",
        content, index);
    send_sse_event(fd, event);
}

/* ── Server thread ──────────────────────────────────────────────── */

static void *server_thread(void *arg) {
    (void)arg;

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("[api-server] socket");
        return NULL;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)g_port);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[api-server] bind");
        close(g_server_fd);
        g_server_fd = -1;
        return NULL;
    }

    if (listen(g_server_fd, API_SERVER_BACKLOG) < 0) {
        perror("[api-server] listen");
        close(g_server_fd);
        g_server_fd = -1;
        return NULL;
    }

    printf("[api-server] OpenAI-compatible API listening on port %d\n", g_port);
    g_running = true;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (g_running) {
        int client_fd = accept(g_server_fd,
                                (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!g_running) break;
            perror("[api-server] accept");
            continue;
        }

        handle_client(client_fd);
    }

    close(g_server_fd);
    g_server_fd = -1;
    printf("[api-server] stopped\n");
    return NULL;
}

/* ── Public API ─────────────────────────────────────────────────── */

void api_server_set_port(int port) {
    g_port = port > 0 ? port : API_DEFAULT_PORT;
}

bool api_server_start(int port, const hermes_config_t *cfg, agent_state_t *agent) {
    if (g_running) return true;

    g_port = port > 0 ? port : API_DEFAULT_PORT;
    g_cfg = (hermes_config_t *)cfg;
    g_agent = agent;
    g_start_time = time(NULL);

    if (pthread_create(&g_thread, NULL, server_thread, NULL) != 0) {
        fprintf(stderr, "[api-server] failed to create thread\n");
        return false;
    }

    /* Wait for thread to signal readiness (bind+listen succeeded or failed) */
    for (int i = 0; i < 50; i++) {
        if (g_running) return true;
        if (g_server_fd < 0 && i > 5) break; /* thread failed, don't keep waiting */
        usleep(100000); /* 100ms intervals, up to 5 seconds */
    }

    if (!g_running) {
        fprintf(stderr, "[api-server] server thread failed to start (bind error?)\n");
        pthread_detach(g_thread);
        return false;
    }

    return true;
}

void api_server_stop(void) {
    if (!g_running) return;
    g_running = false;

    /* Wake up accept() by connecting briefly */
    int tmp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tmp_fd >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((uint16_t)g_port);
        connect(tmp_fd, (struct sockaddr *)&addr, sizeof(addr));
        close(tmp_fd);
    }

    pthread_join(g_thread, NULL);
}

bool api_server_is_running(void) {
    return g_running;
}
