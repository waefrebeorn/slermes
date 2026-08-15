/**
 * api_server_adapter_handlers.c — Core HTTP handlers for API server adapter.
 * Port of Python: gateway/platforms/api_server.py
 */

#include "api_server_adapter.h"
#include "hermes_json.h"
#include "hermes_gateway_webhook.h"
#include "hermes_gateway_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ── HTTP Response Helpers ────────────────────────────────────────── */

void send_json_response(int fd, int status, const char *json_body) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, Idempotency-Key\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, json_body ? strlen(json_body) : 0);
    write(fd, header, n);
    if (json_body) write(fd, json_body, strlen(json_body));
}

void send_error_response(int fd, int status, const char *message, const char *code) {
    char body[1024];
    snprintf(body, sizeof(body),
        "{\"error\":{\"message\":\"%s\",\"type\":\"invalid_request_error\"%s}}",
        message, code ? ",\"code\":\"" : "");
    if (code) {
        char *p = body + strlen(body) - 2;
        snprintf(p, sizeof(body) - (p - body), "%s\"}}", code);
    }
    send_json_response(fd, status, body);
}

void send_sse_headers(int fd) {
    const char *headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    write(fd, headers, strlen(headers));
}

void sse_write_event_fd(int fd, const char *event_type, const char *data) {
    if (event_type) {
        dprintf(fd, "event: %s\n", event_type);
    }
    dprintf(fd, "data: %s\n\n", data ? data : "");
}

/* Port of Python gateway/platforms/qqbot/utils.py:_get_hermes_version(). */
/* ── Helper: Get Herm Version ─────────────────────────────────────── */

/* PoP: get_hermes_version @ gateway/platforms/api_server.py:_hermes_version */
static const char *get_hermes_version(void) {
    /* Would use hermes_cli.__version__ or package metadata */
    return "dev";
}

/* ── Helper: JSON Builders ────────────────────────────────────────── */

static json_t *build_error_json(const char *message, const char *code, const char *param) {
    json_t *root = json_object();
    json_t *err = json_object();
    json_set(err, "message", json_string(message));
    json_set(err, "type", json_string("invalid_request_error"));
    if (code) json_set(err, "code", json_string(code));
    if (param) json_set(err, "param", json_string(param));
    json_set(root, "error", err);
    return root;
}

static void send_json_error(int fd, int status, const char *message, const char *code, const char *param) {
    json_t *err = build_error_json(message, code, param);
    char *out = json_serialize(err);
    send_json_response(fd, status, out);
    free(out);
    json_free(err);
}

/* Port of Python gateway/platforms/api_server.py:_origin_allowed(). */
/* ── Helper: Check if origin is allowed ───────────────────────────── */

static bool origin_allowed(api_server_adapter_t *adapter, const char *origin) {
    if (!origin || !*origin) return true;
    if (adapter->cors_count == 0) return false;

    for (int i = 0; i < adapter->cors_count; i++) {
        if (strcmp(adapter->cors_origins[i], "*") == 0 ||
            strcmp(adapter->cors_origins[i], origin) == 0) {
            return true;
        }
    }
    return false;
}

static void add_cors_headers(api_server_adapter_t *adapter, const char *origin, json_t *root) {
    if (!origin || !*origin || adapter->cors_count == 0) return;
    if (!origin_allowed(adapter, origin)) return;

    json_t *headers = json_object();
    json_set(headers, "Access-Control-Allow-Origin", json_string(origin));
    json_set(headers, "Vary", json_string("Origin"));
    json_set(headers, "Access-Control-Max-Age", json_string("600"));
    json_set(root, "_cors_headers", headers);
}

/* Port of Python gateway/platforms/api_server.py:_handle_health(). */

/* PoP: api_server_handle_health @ gateway/platforms/api_server.py:_handle_health */
void api_server_handle_health(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    (void)origin;

    json_t *root = json_object();
    json_set(root, "status", json_string("ok"));
    json_set(root, "service", json_string("hermes-api-server"));
    json_set(root, "version", json_string(get_hermes_version()));
    json_set(root, "timestamp", json_number(time(NULL)));

    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_capabilities(). */

/* PoP: api_server_handle_capabilities @ gateway/platforms/api_server.py:_handle_capabilities */
void api_server_handle_capabilities(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    (void)origin;

    json_t *root = json_object();
    json_set(root, "streaming", json_bool(true));
    json_set(root, "chat_completions", json_bool(true));
    json_set(root, "responses_api", json_bool(true));
    json_set(root, "runs_api", json_bool(true));
    json_set(root, "sessions_api", json_bool(true));
    json_set(root, "cron_api", json_bool(true));
    json_set(root, "tools", json_bool(true));
    json_set(root, "skills", json_bool(true));
    json_set(root, "vision", json_bool(true));
    json_set(root, "audio", json_bool(false));
    json_set(root, "max_context", json_number(200000));

    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_models(). */

/* PoP: api_server_handle_models @ gateway/platforms/api_server.py:_handle_models */
void api_server_handle_models(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    (void)origin;

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_t *data = json_array();

    json_t *model = json_object();
    json_set(model, "id", json_string(adapter->model_name));
    json_set(model, "object", json_string("model"));
    json_set(model, "owned_by", json_string("hermes"));
    json_append(data, model);

    json_set(root, "data", data);
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_skills() + _handle_toolsets(). */

/* PoP: api_server_handle_skills @ gateway/platforms/api_server.py:_handle_skills */
void api_server_handle_skills(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    (void)origin;

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_t *data = json_array();

    json_t *skill = json_object();
    json_set(skill, "name", json_string("example_skill"));
    json_set(skill, "description", json_string("Example skill for testing"));
    json_append(data, skill);

    json_set(root, "data", data);
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_toolsets(). */
/* PoP: api_server_handle_toolsets @ gateway/platforms/api_server.py:_handle_toolsets */
void api_server_handle_toolsets(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    (void)origin;

    json_t *root = json_object();
    json_set(root, "object", json_string("list"));
    json_t *data = json_array();

    json_t *toolset = json_object();
    json_set(toolset, "name", json_string("default"));
    json_set(toolset, "description", json_string("Default toolset"));
    json_append(data, toolset);

    json_set(root, "data", data);
    char *out = json_serialize(root);
    send_json_response(client_fd, 200, out);
    free(out);
    json_free(root);
}

/* Port of Python gateway/platforms/api_server.py:_handle_options(). */

void api_server_handle_options(api_server_adapter_t *adapter, int client_fd, const char *headers, const char *origin) {
    (void)headers;
    json_t *root = json_object();
    send_json_response(client_fd, 204, "");
    json_free(root);
}

/* End of api_server_adapter_handlers.c */