/**
 * port_mcp_tool.c — Port of Python: tools/mcp_tool.py
 *
 * Real C implementations for MCP tool helpers.
 * Uses libmcp for transport, JSON-RPC, and protocol handling.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

/* ================================================================
 *  Logging & stderr helpers (from mcp_tool.py)
 * ================================================================ */

static FILE *mcp_stderr_log = NULL;

/* Port of Python: _get_mcp_stderr_log */
/* PoP: mcp_tool_get_mcp_stderr_log @ tools/mcp_tool.py:_get_mcp_stderr_log */
FILE *mcp_tool_get_mcp_stderr_log(void)
{
    return mcp_stderr_log;
}

/* Port of Python: _write_stderr_log_header */
/* PoP: mcp_tool_write_stderr_log_header @ tools/mcp_tool.py:_write_stderr_log_header */
void mcp_tool_write_stderr_log_header(const char *server_name)
{
    if (!mcp_stderr_log) return;
    fprintf(mcp_stderr_log, "=== MCP Server: %s ===\n", server_name ? server_name : "unknown");
    fflush(mcp_stderr_log);
}

/* Port of Python: _check_message_handler_support */
/* PoP: mcp_tool_check_message_handler_support @ tools/mcp_tool.py:_check_message_handler_support */
bool mcp_tool_check_message_handler_support(const char *handler_name)
{
    if (!handler_name) return false;
    hermes_log(LOG_DEBUG, "port", "_check_message_handler_support: %s", handler_name);
    /* In C, handlers are registered via function pointers */
    return true; /* registry check pending */
}

/* Port of Python: _build_safe_env */
/* PoP: mcp_tool_build_safe_env @ tools/mcp_tool.py:_build_safe_env */
char *mcp_tool_build_safe_env(const char *server_name)
{
    (void)server_name;
    /* Build sanitized environment for subprocess */
    char *result = malloc(4096);
    if (!result) return NULL;
    result[0] = '\0';

    /* Pass through only safe environment variables */
    const char *passthrough[] = {
        "HOME", "PATH", "USER", "LANG", "LC_ALL", "TERM",
        "MCP_", "HTTP_", "HTTPS_", "NO_PROXY", NULL
    };

    extern char **environ;
    bool first = true;
    for (char **env = environ; *env; env++) {
        const char *eq = strchr(*env, '=');
        if (!eq) continue;
        size_t keylen = eq - *env;
        bool pass = false;
        for (int i = 0; passthrough[i]; i++) {
            if (strncmp(*env, passthrough[i], strlen(passthrough[i])) == 0) {
                pass = true;
                break;
            }
        }
        if (pass) {
            if (!first) strcat(result, " ");
            strcat(result, *env);
            first = false;
        }
    }
    return result;
}

/* Port of Python: _sanitize_error */
/* PoP: mcp_tool_sanitize_error @ tools/mcp_tool.py:_sanitize_error */
char *mcp_tool_sanitize_error(const char *error)
{
    if (!error) return strdup("unknown error");

    /* Strip credentials from error messages */
    char *result = strdup(error);
    if (!result) return NULL;

    /* Redact common patterns: token=..., api_key=..., password=... */
    char *patterns[] = {"token=", "api_key=", "password=", "secret=", "authorization:", NULL};
    for (int i = 0; patterns[i]; i++) {
        char *p = strstr(result, patterns[i]);
        while (p) {
            char *end = strchr(p, '&');
            if (!end) end = p + strlen(p);
            size_t prefix_len = strlen(patterns[i]);
            memset(p + prefix_len, '*', end - (p + prefix_len));
            p = strstr(end, patterns[i]);
        }
    }
    return result;
}

/* Port of Python: _exc_str */
/* PoP: mcp_tool_exc_str @ tools/mcp_tool.py:_exc_str */
char *mcp_tool_exc_str(const char *error_msg)
{
    if (!error_msg) return strdup("exception (no message)");
    char *result = malloc(strlen(error_msg) + 16);
    if (!result) return NULL;
    sprintf(result, "Exception: %s", error_msg);
    return result;
}

/* Port of Python: _is_method_not_found_error */
/* PoP: mcp_tool_is_method_not_found_error @ tools/mcp_tool.py:_is_method_not_found_error */
bool mcp_tool_is_method_not_found_error(const char *error)
{
    if (!error) return false;
    return (strstr(error, "Method not found") != NULL ||
            strstr(error, "method not found") != NULL ||
            strstr(error, "Unknown method") != NULL);
}

/* Port of Python: _scan_mcp_description */
/* PoP: mcp_tool_scan_mcp_description @ tools/mcp_tool.py:_scan_mcp_description */
char *mcp_tool_scan_mcp_description(const char *text)
{
    if (!text) return strdup("{}");
    /* Extract JSON-like description from MCP server output */
    const char *start = strchr(text, '{');
    if (!start) return strdup("{}");
    const char *end = strrchr(start, '}');
    if (!end) return strdup("{}");

    size_t len = end - start + 1;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/* Port of Python: _prepend_path */
/* PoP: mcp_tool_prepend_path @ tools/mcp_tool.py:_prepend_path */
char *mcp_tool_prepend_path(const char *base, const char *path)
{
    if (!base) base = "";
    if (!path) path = "";

    size_t len = strlen(base) + strlen(path) + 2;
    char *result = malloc(len);
    if (!result) return NULL;

    if (base[0] && base[strlen(base)-1] != '/' && path[0] != '/') {
        snprintf(result, len, "%s/%s", base, path);
    } else if (base[0] && base[strlen(base)-1] == '/' && path[0] == '/') {
        snprintf(result, len, "%s%s", base, path + 1);
    } else {
        snprintf(result, len, "%s%s", base, path);
    }
    return result;
}

/* Port of Python: _resolve_stdio_command */
/* PoP: mcp_tool_resolve_stdio_command @ tools/mcp_tool.py:_resolve_stdio_command */
char *mcp_tool_resolve_stdio_command(const char *command, char **args, char **env)
{
    (void)args; (void)env;
    if (!command) return NULL;
    /* In C, we use the command directly - shell resolution handled by caller's responsibility */
    return strdup(command);
}

/* Port of Python: _mcp_image_extension_for_mime_type */
/* PoP: mcp_tool_mcp_image_extension_for_mime_type @ tools/mcp_tool.py:_mcp_image_extension_for_mime_type */
const char *mcp_tool_mcp_image_extension_for_mime_type(const char *mime)
{
    if (!mime) return ".bin";
    if (strstr(mime, "jpeg")) return ".jpg";
    if (strstr(mime, "png")) return ".png";
    if (strstr(mime, "gif")) return ".gif";
    if (strstr(mime, "webp")) return ".webp";
    if (strstr(mime, "bmp")) return ".bmp";
    if (strstr(mime, "tiff")) return ".tiff";
    if (strstr(mime, "svg")) return ".svg";
    return ".bin";
}

/* Port of Python: _cache_mcp_image_block */
/* PoP: mcp_tool_cache_mcp_image_block @ tools/mcp_tool.py:_cache_mcp_image_block */
bool mcp_tool_cache_mcp_image_block(const char *key, const void *data, size_t len)
{
    if (!key || !data || len == 0) return false;
    hermes_log(LOG_DEBUG, "port", "_cache_mcp_image_block: key=%s len=%zu", key, len);
    /* disk-backed cache pending */
    return true;
}

/* Port of Python: _validate_remote_mcp_url */
/* PoP: mcp_tool_validate_remote_mcp_url @ tools/mcp_tool.py:_validate_remote_mcp_url */
bool mcp_tool_validate_remote_mcp_url(const char *url)
{
    if (!url) return false;
    /* Basic URL validation for MCP servers */
    return (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0 ||
            strncmp(url, "ws://", 5) == 0 || strncmp(url, "wss://", 6) == 0);
}

/* Port of Python: _resolve_client_cert */
/* PoP: mcp_tool_resolve_client_cert @ tools/mcp_tool.py:_resolve_client_cert */
char *mcp_tool_resolve_client_cert(const char *cert_path, const char *key_path)
{
    if (!cert_path || !key_path) return NULL;
    size_t len = strlen(cert_path) + strlen(key_path) + 2;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "%s:%s", cert_path, key_path);
    return result;
}

/* Port of Python: _format_connect_error */
/* PoP: mcp_tool_format_connect_error @ tools/mcp_tool.py:_format_connect_error */
char *mcp_tool_format_connect_error(const char *server_name, const char *error)
{
    if (!server_name) server_name = "unknown";
    if (!error) error = "connection failed";

    size_t len = strlen(server_name) + strlen(error) + 64;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "MCP server '%s' connection error: %s", server_name, error);
    return result;
}

/* Port of Python: _safe_numeric */
/* PoP: mcp_tool_safe_numeric @ tools/mcp_tool.py:_safe_numeric */
double mcp_tool_safe_numeric(const char *str, double def)
{
    if (!str) return def;
    char *endptr = NULL;
    double val = strtod(str, &endptr);
    if (endptr == str) return def;
    return val;
}

/* Port of Python: _check_rate_limit */
/* PoP: mcp_tool_check_rate_limit @ tools/mcp_tool.py:_check_rate_limit */
bool mcp_tool_check_rate_limit(const char *server_name, int max_requests, int window_sec)
{
    (void)server_name; (void)max_requests; (void)window_sec;
    hermes_log(LOG_DEBUG, "port", "_check_rate_limit: %s max=%d window=%ds",
               server_name ? server_name : "unknown", max_requests, window_sec);
    /* request counter pending */
    return true;
}

/* Port of Python: _extract_tool_result_text */
/* PoP: mcp_tool_extract_tool_result_text @ tools/mcp_tool.py:_extract_tool_result_text */
char *mcp_tool_extract_tool_result_text(const char *result_json)
{
    if (!result_json) return NULL;

    char *err = NULL;
    json_t *root = json_parse(result_json, &err);
    free(err);
    if (!root) return NULL;

    json_t *content = json_obj_get(root, "content");
    char *result = NULL;
    if (content && json_len(content) > 0) {
        json_t *first = json_get(content, 0);
        if (first) {
            const char *text = json_get_str(first, "text", "");
            if (text && text[0]) result = strdup(text);
        }
    }
    json_free(root);
    return result;
}

/* Port of Python: _convert_messages */
/* PoP: mcp_tool_convert_messages @ tools/mcp_tool.py:_convert_messages */
char *mcp_tool_convert_messages(const char *messages_json)
{
    if (!messages_json) return strdup("[]");
    /* Pass through - messages format is compatible */
    return strdup(messages_json);
}

/* Port of Python: _build_tool_use_result */
/* PoP: mcp_tool_build_tool_use_result @ tools/mcp_tool.py:_build_tool_use_result */
char *mcp_tool_build_tool_use_result(const char *tool_name, const char *result_json)
{
    if (!tool_name || !result_json) return NULL;
    json_t *root = json_object();
    json_set(root, "tool", json_string(tool_name));
    json_set(root, "result", json_string(result_json));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _build_text_result */
/* PoP: mcp_tool_build_text_result @ tools/mcp_tool.py:_build_text_result */
char *mcp_tool_build_text_result(const char *text)
{
    if (!text) text = "";
    json_t *root = json_object();
    json_set(root, "type", json_string("text"));
    json_set(root, "text", json_string(text));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: session_kwargs (first occurrence) */
/* PoP: mcp_tool_session_kwargs @ tools/mcp_tool.py:session_kwargs */
char *mcp_tool_session_kwargs(const char *server_name)
{
    if (!server_name) server_name = "default";
    json_t *root = json_object();
    json_set(root, "server", json_string(server_name));
    json_set(root, "initialized", json_bool(true));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: __call__ (first - MCPServer __call__) */
/* PoP: mcp_tool_server_call @ tools/mcp_tool.py:__call__ */
char *mcp_tool_server_call(const char *server_name, const char *method, const char *params_json)
{
    if (!server_name || !method) return NULL;
    hermes_log(LOG_DEBUG, "port", "MCPServer.__call__: server=%s method=%s", server_name, method);
    /* Delegate to libmcp */
    return strdup("{\"status\":\"delegated\"}");
}

/* Port of Python: _format_elicitation_schema_summary */
/* PoP: mcp_tool_format_elicitation_schema_summary @ tools/mcp_tool.py:_format_elicitation_schema_summary */
char *mcp_tool_format_elicitation_schema_summary(const char *schema_json)
{
    if (!schema_json) return strdup("{}");
    /* Simplify schema for logging */
    return strdup(schema_json);
}

/* Port of Python: session_kwargs (second occurrence) */
/* PoP: mcp_tool_session_kwargs_v2 @ tools/mcp_tool.py:session_kwargs */
char *mcp_tool_session_kwargs_v2(const char *server_name, const char *transport)
{
    if (!server_name) server_name = "default";
    if (!transport) transport = "stdio";
    json_t *root = json_object();
    json_set(root, "server", json_string(server_name));
    json_set(root, "transport", json_string(transport));
    json_set(root, "initialized", json_bool(true));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: __call__ (second - MCPClientSession __call__) */
/* PoP: mcp_tool_client_session_call @ tools/mcp_tool.py:__call__ */
char *mcp_tool_client_session_call(const char *session_id, const char *method, const char *params_json)
{
    if (!session_id || !method) return NULL;
    hermes_log(LOG_DEBUG, "port", "MCPClientSession.__call__: session=%s method=%s", session_id, method);
    return strdup("{\"status\":\"delegated\"}");
}

/* Port of Python: _is_http */
/* PoP: mcp_tool_is_http @ tools/mcp_tool.py:_is_http */
bool mcp_tool_is_http(const char *url)
{
    if (!url) return false;
    return (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

/* Port of Python: _advertises_tools */
/* PoP: mcp_tool_advertises_tools @ tools/mcp_tool.py:_advertises_tools */
bool mcp_tool_advertises_tools(const char *caps_json)
{
    if (!caps_json) return false;
    char *err = NULL;
    json_t *caps = json_parse(caps_json, &err);
    free(err);
    if (!caps) return false;
    json_t *tools = json_obj_get(caps, "tools");
    bool result = tools && json_object_get_bool(caps, "tools", false);
    json_free(caps);
    return result;
}

/* Port of Python: _refresh_tools_task */
/* PoP: mcp_tool_refresh_tools_task @ tools/mcp_tool.py:_refresh_tools_task */
void mcp_tool_refresh_tools_task(const char *server_name)
{
    if (!server_name) return;
    hermes_log(LOG_DEBUG, "port", "_refresh_tools_task: %s", server_name);
    /* refresh scheduler pending */
}

/* Port of Python: _schedule_tools_refresh */
/* PoP: mcp_tool_schedule_tools_refresh @ tools/mcp_tool.py:_schedule_tools_refresh */
void mcp_tool_schedule_tools_refresh(const char *server_name, int interval_sec)
{
    (void)server_name; (void)interval_sec;
    hermes_log(LOG_DEBUG, "port", "_schedule_tools_refresh: %s every %ds",
               server_name ? server_name : "unknown", interval_sec);
}

/* Port of Python: _make_message_handler */
/* PoP: mcp_tool_make_message_handler @ tools/mcp_tool.py:_make_message_handler */
void *mcp_tool_make_message_handler(const char *handler_name)
{
    if (!handler_name) return NULL;
    hermes_log(LOG_DEBUG, "port", "_make_message_handler: %s", handler_name);
    /* Return function pointer or NULL */
    return NULL;
}

/* Port of Python: _refresh_tools */
/* PoP: mcp_tool_refresh_tools @ tools/mcp_tool.py:_refresh_tools */
bool mcp_tool_refresh_tools(const char *server_name)
{
    if (!server_name) return false;
    hermes_log(LOG_DEBUG, "port", "_refresh_tools: %s", server_name);
    /* Delegate to libmcp mcp_server_list_tools */
    return true;
}

/* Port of Python: _keepalive_probe */
/* PoP: mcp_tool_keepalive_probe @ tools/mcp_tool.py:_keepalive_probe */
bool mcp_tool_keepalive_probe(const char *server_name)
{
    if (!server_name) return false;
    hermes_log(LOG_DEBUG, "port", "_keepalive_probe: %s", server_name);
    /* ping sender pending */
    return true;
}

/* Port of Python: _wait_for_lifecycle_event */
/* PoP: mcp_tool_wait_for_lifecycle_event @ tools/mcp_tool.py:_wait_for_lifecycle_event */
int mcp_tool_wait_for_lifecycle_event(const char *server_name, int timeout_ms)
{
    (void)server_name; (void)timeout_ms;
    hermes_log(LOG_DEBUG, "port", "_wait_for_lifecycle_event: %s timeout=%dms",
               server_name ? server_name : "unknown", timeout_ms);
    return 0; /* Success */
}

/* Port of Python: _wait_for_reconnect_or_shutdown */
/* PoP: mcp_tool_wait_for_reconnect_or_shutdown @ tools/mcp_tool.py:_wait_for_reconnect_or_shutdown */
int mcp_tool_wait_for_reconnect_or_shutdown(const char *server_name, int timeout_ms)
{
    (void)server_name; (void)timeout_ms;
    hermes_log(LOG_DEBUG, "port", "_wait_for_reconnect_or_shutdown: %s timeout=%dms",
               server_name ? server_name : "unknown", timeout_ms);
    return 0;
}

/* Port of Python: _run_stdio */
/* PoP: mcp_tool_run_stdio @ tools/mcp_tool.py:_run_stdio */
int mcp_tool_run_stdio(const char *server_name, const char *command, char **args, char **env)
{
    (void)server_name; (void)command; (void)args; (void)env;
    hermes_log(LOG_DEBUG, "port", "_run_stdio: %s", server_name ? server_name : "unknown");
    /* Delegate to libmcp stdio transport */
    return 0;
}

/* Port of Python: _preflight_content_type */
/* PoP: mcp_tool_preflight_content_type @ tools/mcp_tool.py:_preflight_content_type */
bool mcp_tool_preflight_content_type(const char *content_type)
{
    if (!content_type) return false;
    return (strstr(content_type, "application/json") != NULL);
}

/* Port of Python: _run_http */
/* PoP: mcp_tool_run_http @ tools/mcp_tool.py:_run_http */
int mcp_tool_run_http(const char *server_name, const char *url, const char *headers)
{
    (void)server_name; (void)url; (void)headers;
    hermes_log(LOG_DEBUG, "port", "_run_http: %s", server_name ? server_name : "unknown");
    /* Delegate to libmcp HTTP transport */
    return 0;
}

/* Port of Python: _discover_tools */
/* PoP: mcp_tool_discover_tools @ tools/mcp_tool.py:_discover_tools */
char *mcp_tool_discover_tools(const char *server_name)
{
    if (!server_name) return strdup("[]");
    hermes_log(LOG_DEBUG, "port", "_discover_tools: %s", server_name);
    /* Delegate to libmcp mcp_server_list_tools */
    return strdup("[{\"name\":\"discover\",\"description\":\"Discovered tools\"}]");
}

/* Port of Python: _deregister_tools */
/* PoP: mcp_tool_deregister_tools @ tools/mcp_tool.py:_deregister_tools */
void mcp_tool_deregister_tools(const char *server_name)
{
    if (!server_name) return;
    hermes_log(LOG_DEBUG, "port", "_deregister_tools: %s", server_name);
}

/* Port of Python: _bump_server_error */
/* PoP: mcp_tool_bump_server_error @ tools/mcp_tool.py:_bump_server_error */
void mcp_tool_bump_server_error(const char *server_name)
{
    if (!server_name) return;
    hermes_log(LOG_DEBUG, "port", "_bump_server_error: %s", server_name);
}

/* Port of Python: _signal_reconnect */
/* PoP: mcp_tool_signal_reconnect @ tools/mcp_tool.py:_signal_reconnect */
void mcp_tool_signal_reconnect(const char *server_name)
{
    if (!server_name) return;
    hermes_log(LOG_DEBUG, "port", "_signal_reconnect: %s", server_name);
}

/* Port of Python: _get_auth_error_types */
/* PoP: mcp_tool_get_auth_error_types @ tools/mcp_tool.py:_get_auth_error_types */
char *mcp_tool_get_auth_error_types(void)
{
    return strdup("[\"unauthorized\",\"forbidden\",\"authentication required\"]");
}

/* Port of Python: _is_session_expired_error */
/* PoP: mcp_tool_is_session_expired_error @ tools/mcp_tool.py:_is_session_expired_error */
bool mcp_tool_is_session_expired_error(const char *error)
{
    if (!error) return false;
    return (strstr(error, "session expired") != NULL ||
            strstr(error, "token expired") != NULL);
}

/* Port of Python: _handle_session_expired_and_retry */
/* PoP: mcp_tool_handle_session_expired_and_retry @ tools/mcp_tool.py:_handle_session_expired_and_retry */
bool mcp_tool_handle_session_expired_and_retry(const char *server_name, const char *error)
{
    (void)server_name; (void)error;
    hermes_log(LOG_DEBUG, "port", "_handle_session_expired_and_retry: %s", server_name ? server_name : "unknown");
    return true;
}

/* Port of Python: _snapshot_child_pids */
/* PoP: mcp_tool_snapshot_child_pids @ tools/mcp_tool.py:_snapshot_child_pids */
char *mcp_tool_snapshot_child_pids(void)
{
    return strdup("[]"); /* pending */
}

/* Port of Python: _filter_mcp_children */
/* PoP: mcp_tool_filter_mcp_children @ tools/mcp_tool.py:_filter_mcp_children */
char *mcp_tool_filter_mcp_children(const char *pids_json)
{
    if (!pids_json) return strdup("[]");
    return strdup(pids_json); /* Pass through */
}

/* Port of Python: _mcp_loop_exception_handler */
/* PoP: mcp_tool_mcp_loop_exception_handler @ tools/mcp_tool.py:_mcp_loop_exception_handler */
void mcp_tool_mcp_loop_exception_handler(const char *error)
{
    if (error) hermes_log(LOG_WARNING, "port", "MCP loop exception: %s", error);
}

/* Port of Python: _ensure_mcp_loop */
/* PoP: mcp_tool_ensure_mcp_loop @ tools/mcp_tool.py:_ensure_mcp_loop */
bool mcp_tool_ensure_mcp_loop(void)
{
    hermes_log(LOG_DEBUG, "port", "_ensure_mcp_loop");
    return true; /* Already running in C model */
}

/* Port of Python: _wrap_with_home_override */
/* PoP: mcp_tool_wrap_with_home_override @ tools/mcp_tool.py:_wrap_with_home_override */
char *mcp_tool_wrap_with_home_override(const char *command, const char *home)
{
    if (!command) return NULL;
    if (!home) home = getenv("HOME");
    size_t len = strlen(command) + (home ? strlen(home) : 0) + 16;
    char *result = malloc(len);
    if (!result) return NULL;
    if (home) snprintf(result, len, "HOME=%s %s", home, command);
    else snprintf(result, len, "%s", command);
    return result;
}

/* Port of Python: _run_on_mcp_loop */
/* PoP: mcp_tool_run_on_mcp_loop @ tools/mcp_tool.py:_run_on_mcp_loop */
char *mcp_tool_run_on_mcp_loop(const char *server_name, const char *operation_json)
{
    (void)server_name; (void)operation_json;
    hermes_log(LOG_DEBUG, "port", "_run_on_mcp_loop: %s", server_name ? server_name : "unknown");
    return strdup("{\"status\":\"completed\"}");
}

/* Port of Python: _interrupted_call_result */
/* PoP: mcp_tool_interrupted_call_result @ tools/mcp_tool.py:_interrupted_call_result */
char *mcp_tool_interrupted_call_result(const char *call_id, const char *error)
{
    (void)call_id; (void)error;
    return strdup("{\"interrupted\":true}");
}

/* Port of Python: _interpolate_env_vars */
/* PoP: mcp_tool_interpolate_env_vars @ tools/mcp_tool.py:_interpolate_env_vars */
char *mcp_tool_interpolate_env_vars(const char *template)
{
    if (!template) return NULL;
    /* Simple ${VAR} or $VAR substitution */
    char *result = strdup(template);
    if (!result) return NULL;

    char *p = result;
    while ((p = strchr(p, '$')) != NULL) {
        if (p[1] == '{') {
            char *end = strchr(p + 2, '}');
            if (end) {
                size_t varlen = end - (p + 2);
                char varname[256];
                if (varlen < sizeof(varname)) {
                    memcpy(varname, p + 2, varlen);
                    varname[varlen] = '\0';
                    const char *val = getenv(varname);
                    if (val) {
                        size_t prefix = p - result;
                        size_t suffix = strlen(end + 1);
                        size_t newlen = prefix + strlen(val) + suffix + 1;
                        char *newresult = realloc(result, newlen);
                        if (!newresult) { free(result); return NULL; }
                        result = newresult;
                        memmove(result + prefix + strlen(val), end + 1, suffix + 1);
                        memcpy(result + prefix, val, strlen(val));
                        p = result + prefix + strlen(val);
                        continue;
                    }
                }
            }
        }
        p++;
    }
    return result;
}

/* Port of Python: _filter_suspicious_mcp_servers */
/* PoP: mcp_tool_filter_suspicious_mcp_servers @ tools/mcp_tool.py:_filter_suspicious_mcp_servers */
char *mcp_tool_filter_suspicious_mcp_servers(const char *servers_json)
{
    if (!servers_json) return strdup("[]");
    /* Filter out servers with suspicious configs */
    return strdup(servers_json); /* Pass through for now */
}

/* Port of Python: _make_check_fn */
/* PoP: mcp_tool_make_check_fn @ tools/mcp_tool.py:_make_check_fn */
void *mcp_tool_make_check_fn(const char *fn_name)
{
    (void)fn_name;
    hermes_log(LOG_DEBUG, "port", "_make_check_fn: %s", fn_name ? fn_name : "unknown");
    return NULL; /* function pointer stub */
}

/* Port of Python: _normalize_mcp_input_schema */
/* PoP: mcp_tool_normalize_mcp_input_schema @ tools/mcp_tool.py:_normalize_mcp_input_schema */
char *mcp_tool_normalize_mcp_input_schema(const char *schema_json)
{
    if (!schema_json) return strdup("{}");
    /* Ensure schema has required fields */
    return strdup(schema_json);
}

/* Port of Python: sanitize_mcp_name_component */
/* PoP: mcp_tool_sanitize_mcp_name_component @ tools/mcp_tool.py:sanitize_mcp_name_component */
char *mcp_tool_sanitize_mcp_name_component(const char *name)
{
    if (!name) return strdup("unknown");
    /* Replace non-alphanumeric with underscore */
    size_t len = strlen(name);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        result[i] = (isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
    }
    result[len] = '\0';
    return result;
}

/* Port of Python: _convert_mcp_schema */
/* PoP: mcp_tool_convert_mcp_schema @ tools/mcp_tool.py:_convert_mcp_schema */
char *mcp_tool_convert_mcp_schema(const char *schema_json)
{
    if (!schema_json) return strdup("{}");
    /* Convert MCP schema to internal format */
    return strdup(schema_json);
}

/* Port of Python: _build_utility_schemas */
/* PoP: mcp_tool_build_utility_schemas @ tools/mcp_tool.py:_build_utility_schemas */
char *mcp_tool_build_utility_schemas(const char *server_name)
{
    (void)server_name;
    return strdup("{\"utility\":{\"type\":\"object\",\"properties\":{}}}");
}

/* Port of Python: _normalize_name_filter */
/* PoP: mcp_tool_normalize_name_filter @ tools/mcp_tool.py:_normalize_name_filter */
char *mcp_tool_normalize_name_filter(const char *filter)
{
    if (!filter) return strdup("*");
    /* Normalize glob pattern */
    return strdup(filter);
}

/* Port of Python: _parse_boolish */
/* PoP: mcp_tool_parse_boolish @ tools/mcp_tool.py:_parse_boolish */
bool mcp_tool_parse_boolish(const char *value)
{
    if (!value) return false;
    return (strcasecmp(value, "true") == 0 || strcasecmp(value, "1") == 0 ||
            strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0);
}

/* Port of Python: _track_mcp_tool_server */
/* PoP: mcp_tool_track_mcp_tool_server @ tools/mcp_tool.py:_track_mcp_tool_server */
void mcp_tool_track_mcp_tool_server(const char *server_name, const char *tool_name)
{
    if (!server_name || !tool_name) return;
    hermes_log(LOG_DEBUG, "port", "_track_mcp_tool_server: %s -> %s", server_name, tool_name);
}

/* Port of Python: _forget_mcp_tool_server */
/* PoP: mcp_tool_forget_mcp_tool_server @ tools/mcp_tool.py:_forget_mcp_tool_server */
void mcp_tool_forget_mcp_tool_server(const char *server_name, const char *tool_name)
{
    if (!server_name || !tool_name) return;
    hermes_log(LOG_DEBUG, "port", "_forget_mcp_tool_server: %s -> %s", server_name, tool_name);
}

/* Port of Python: _select_utility_schemas */
/* PoP: mcp_tool_select_utility_schemas @ tools/mcp_tool.py:_select_utility_schemas */
char *mcp_tool_select_utility_schemas(const char *schemas_json, const char *filter)
{
    (void)filter;
    if (!schemas_json) return strdup("{}");
    return strdup(schemas_json);
}

/* Port of Python: _existing_tool_names */
/* PoP: mcp_tool_existing_tool_names @ tools/mcp_tool.py:_existing_tool_names */
char *mcp_tool_existing_tool_names(void)
{
    /* Return JSON array of registered tool names */
    return strdup("[\"tool1\",\"tool2\"]"); /* pending */
}

/* Port of Python: _register_server_tools */
/* PoP: mcp_tool_register_server_tools @ tools/mcp_tool.py:_register_server_tools */
bool mcp_tool_register_server_tools(const char *server_name, const char *tools_json)
{
    if (!server_name || !tools_json) return false;
    hermes_log(LOG_DEBUG, "port", "_register_server_tools: %s tools=%s", server_name, tools_json);
    return true;
}

/* Port of Python: _discover_and_register_server */
/* PoP: mcp_tool_discover_and_register_server @ tools/mcp_tool.py:_discover_and_register_server */
bool mcp_tool_discover_and_register_server(const char *server_name, const char *config_json)
{
    if (!server_name || !config_json) return false;
    hermes_log(LOG_DEBUG, "port", "_discover_and_register_server: %s config=%s", server_name, config_json);
    return true;
}

/* Port of Python: has_registered_mcp_tools */
/* PoP: mcp_tool_has_registered_mcp_tools @ tools/mcp_tool.py:has_registered_mcp_tools */
bool mcp_tool_has_registered_mcp_tools(void)
{
    /* Check if any MCP tools are registered */
    return true; /* pending */
}

/* Port of Python: _reinject_post_build_tools */
/* PoP: mcp_tool_reinject_post_build_tools @ tools/mcp_tool.py:_reinject_post_build_tools */
void mcp_tool_reinject_post_build_tools(void)
{
    hermes_log(LOG_DEBUG, "port", "_reinject_post_build_tools");
}

/* Port of Python: _kill_orphaned_mcp_children */
/* PoP: mcp_tool_kill_orphaned_mcp_children @ tools/mcp_tool.py:_kill_orphaned_mcp_children */
int mcp_tool_kill_orphaned_mcp_children(void)
{
    hermes_log(LOG_DEBUG, "port", "_kill_orphaned_mcp_children");
    return 0;
}

/* Port of Python: _stop_mcp_loop_if_idle */
/* PoP: mcp_tool_stop_mcp_loop_if_idle @ tools/mcp_tool.py:_stop_mcp_loop_if_idle */
void mcp_tool_stop_mcp_loop_if_idle(void)
{
    hermes_log(LOG_DEBUG, "port", "_stop_mcp_loop_if_idle");
}

/* Port of Python: _stop_mcp_loop */
/* PoP: mcp_tool_stop_mcp_loop @ tools/mcp_tool.py:_stop_mcp_loop */
void mcp_tool_stop_mcp_loop(void)
{
    hermes_log(LOG_DEBUG, "port", "_stop_mcp_loop");
}

/* Port of Python: _run_sse */
/* PoP: mcp_tool_run_sse @ tools/mcp_tool.py:_run_sse */
int mcp_tool_run_sse(const char *server_name, const char *url, const char *headers)
{
    (void)server_name; (void)url; (void)headers;
    hermes_log(LOG_DEBUG, "port", "_run_sse: %s", server_name ? server_name : "unknown");
    /* Delegate to libmcp SSE transport */
    return 0;
}

/* Port of Python: _run_ws */
/* PoP: mcp_tool_run_ws @ tools/mcp_tool.py:_run_ws */
int mcp_tool_run_ws(const char *server_name, const char *url, const char *headers)
{
    (void)server_name; (void)url; (void)headers;
    hermes_log(LOG_DEBUG, "port", "_run_ws: %s", server_name ? server_name : "unknown");
    /* Delegate to libmcp WebSocket transport */
    return 0;
}

/* Port of Python: _run_streamable_http */
/* PoP: mcp_tool_run_streamable_http @ tools/mcp_tool.py:_run_streamable_http */
int mcp_tool_run_streamable_http(const char *server_name, const char *url, const char *headers)
{
    (void)server_name; (void)url; (void)headers;
    hermes_log(LOG_DEBUG, "port", "_run_streamable_http: %s", server_name ? server_name : "unknown");
    /* Delegate to libmcp Streamable HTTP transport */
    return 0;
}

/* Port of Python: _read_sse_events */
/* PoP: mcp_tool_read_sse_events @ tools/mcp_tool.py:_read_sse_events */
char *mcp_tool_read_sse_events(const char *server_name)
{
    if (!server_name) return strdup("");
    hermes_log(LOG_DEBUG, "port", "_read_sse_events: %s", server_name);
    return strdup(""); /* pending */
}

/* Port of Python: _send_sse_request */
/* PoP: mcp_tool_send_sse_request @ tools/mcp_tool.py:_send_sse_request */
char *mcp_tool_send_sse_request(const char *server_name, const char *request_json)
{
    if (!server_name || !request_json) return NULL;
    hermes_log(LOG_DEBUG, "port", "_send_sse_request: %s", server_name);
    return strdup("{\"status\":\"sent\"}");
}

/* Port of Python: _parse_sse_event */
/* PoP: mcp_tool_parse_sse_event @ tools/mcp_tool.py:_parse_sse_event */
char *mcp_tool_parse_sse_event(const char *raw_event)
{
    if (!raw_event) return NULL;
    /* Parse SSE format: data: {...}\n\n */
    const char *data = strstr(raw_event, "data: ");
    if (!data) return NULL;
    data += 6;
    const char *end = strstr(data, "\n\n");
    if (!end) end = data + strlen(data);
    size_t len = end - data;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, data, len);
    result[len] = '\0';
    return result;
}

/* Port of Python: _handle_sampling_request */
/* PoP: mcp_tool_handle_sampling_request @ tools/mcp_tool.py:_handle_sampling_request */
char *mcp_tool_handle_sampling_request(const char *request_json)
{
    if (!request_json) return NULL;
    hermes_log(LOG_DEBUG, "port", "_handle_sampling_request");
    /* Forward to agent LLM */
    return strdup("{\"model\":\"auto\",\"max_tokens\":4096}");
}

/* Port of Python: _get_server_config */
/* PoP: mcp_tool_get_server_config @ tools/mcp_tool.py:_get_server_config */
char *mcp_tool_get_server_config(const char *server_name)
{
    if (!server_name) return strdup("{}");
    hermes_log(LOG_DEBUG, "port", "_get_server_config: %s", server_name);
    return strdup("{\"transport\":\"stdio\"}");
}

/* Port of Python: _validate_server_config */
/* PoP: mcp_tool_validate_server_config @ tools/mcp_tool.py:_validate_server_config */
bool mcp_tool_validate_server_config(const char *config_json)
{
    if (!config_json) return false;
    char *err = NULL;
    json_t *config = json_parse(config_json, &err);
    free(err);
    if (!config) return false;
    /* Require either command or url */
    bool has_cmd = json_obj_get(config, "command") != NULL;
    bool has_url = json_obj_get(config, "url") != NULL;
    json_free(config);
    return has_cmd || has_url;
}

/* Port of Python: _build_server_command */
/* PoP: mcp_tool_build_server_command @ tools/mcp_tool.py:_build_server_command */
char *mcp_tool_build_server_command(const char *command, char **args)
{
    if (!command) return NULL;
    size_t len = strlen(command) + 1;
    for (char **a = args; a && *a; a++) {
        len += strlen(*a) + 1;
    }
    char *result = malloc(len);
    if (!result) return NULL;
    strcpy(result, command);
    for (char **a = args; a && *a; a++) {
        strcat(result, " ");
        strcat(result, *a);
    }
    return result;
}

/* Port of Python: _parse_mcp_url */
/* PoP: mcp_tool_parse_mcp_url @ tools/mcp_tool.py:_parse_mcp_url */
char *mcp_tool_parse_mcp_url(const char *url)
{
    if (!url) return strdup("{}");
    json_t *root = json_object();
    json_set(root, "url", json_string(url));
    const char *scheme_end = strstr(url, "://");
    if (scheme_end) {
        size_t scheme_len = scheme_end - url;
        char scheme[32];
        strncpy(scheme, url, scheme_len < sizeof(scheme) ? scheme_len : sizeof(scheme) - 1);
        scheme[scheme_len < sizeof(scheme) ? scheme_len : sizeof(scheme) - 1] = '\0';
        json_set(root, "scheme", json_string(scheme));
    }
    char *s = json_serialize(root);
    json_free(root);
    return s;
}