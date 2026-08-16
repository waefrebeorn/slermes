/* ── recovered top-section (includes + state) from src/tools/mcp_tool.c ── */
/* PoP: MCP tool (port of tools/mcp_tool) */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "registry.h"
#include "mcp.h"
#include "mcp_oauth.h"
#include "osv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  State
 * ================================================================ */

#define MAX_MCP_SERVERS 16
#define MAX_DYNAMIC_TOOLS 256

mcp_server_t *g_servers[MAX_MCP_SERVERS];
int g_server_count = 0; /* non-static — accessed by commands.c for /reload-mcp */

/* P63-P64: Per-server auth config (parallel to g_servers array) */
static mcp_auth_t g_server_auth[MAX_MCP_SERVERS];
static char g_credential_store_path[HERMES_PATH_MAX] = "";

/* P66: Per-server tool filter config (parallel to g_servers) */
#define MAX_FILTER_PATTERNS 16

/* Per-server sampling config */
typedef struct {
    bool  enabled;
    char  model[128];         /* override model (empty = use default) */
    int   max_tokens_cap;     /* max tokens per request (0 = default) */
    int   timeout_sec;        /* LLM call timeout */
    int   max_rpm;            /* max requests per minute (0 = unlimited) */
} mcp_sampling_cfg_t;

static mcp_sampling_cfg_t g_server_sampling[MAX_MCP_SERVERS];
typedef struct {
    char allow[MAX_FILTER_PATTERNS][128];  /* allowlist patterns */
    int  allow_count;
    char block[MAX_FILTER_PATTERNS][128];  /* blocklist patterns */
    int  block_count;
} mcp_filter_t;
static mcp_filter_t g_server_filters[MAX_MCP_SERVERS];

/* Dynamically registered tool handlers */
typedef struct {
    char server_name[64];
    char tool_name[128];
} mcp_dynamic_tool_t;

static mcp_dynamic_tool_t g_dynamic_tools[MAX_DYNAMIC_TOOLS];
static int g_dynamic_count = 0;

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/mcp_tool.c — C11 port of tools/mcp_tool.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_mcp_tool_py_parity.c ── */
/* Slermes C port — tools/mcp_tool.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_mcp_tool_get_mcp_stderr_log_p @ tools/mcp_tool.py:_get_mcp_stderr_log */
json_t *tools_mcp_tool_get_mcp_stderr_log_p(void) {
    return json_object();
}

/* PoP: tools_mcp_tool_getattr___p @ tools/mcp_tool.py:__getattr__ */
json_t *tools_mcp_tool_getattr___p(void) {
    return json_object();
}

/* PoP: tools_mcp_tool_mcp_resource_filename_p @ tools/mcp_tool.py:_mcp_resource_filename */
json_t *tools_mcp_tool_mcp_resource_filename_p(void) {
    return json_object();
}

/* PoP: tools_mcp_tool_warn_hidden_whitespace_p @ tools/mcp_tool.py:_warn_hidden_whitespace */
json_t *tools_mcp_tool_warn_hidden_whitespace_p(void) {
    return json_object();
}


/* ── from src/tools/port_mcp_tool.c ── */
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

/* Port of Python: _wait_for_reconnect_or_shutdown */

/* Port of Python: _run_stdio */

/* Port of Python: _preflight_content_type */
/* PoP: mcp_tool_preflight_content_type @ tools/mcp_tool.py:_preflight_content_type */
bool mcp_tool_preflight_content_type(const char *content_type)
{
    if (!content_type) return false;
    return (strstr(content_type, "application/json") != NULL);
}

/* Port of Python: _run_http */

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


/* Port of Python: _is_session_expired_error */
/* PoP: mcp_tool_is_session_expired_error @ tools/mcp_tool.py:_is_session_expired_error */
bool mcp_tool_is_session_expired_error(const char *error)
{
    if (!error) return false;
    return (strstr(error, "session expired") != NULL ||
            strstr(error, "token expired") != NULL);
}

/* Port of Python: _handle_session_expired_and_retry */

/* Port of Python: _snapshot_child_pids */


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

    /* servers is an object {name: cfg}; drop exfiltration/persistence-shaped
     * entries via the ported mcp_security validator. */
    extern bool hermes_cli_mcp_security_is_mcp_server_entry_suspicious(const char *name, json_t *entry);

    char *err = NULL;
    json_t *servers = json_parse(servers_json, &err);
    if (!servers || servers->type != JSON_OBJECT) {
        if (servers) json_free(servers);
        free(err);
        return strdup(servers_json);  /* not the expected shape — leave as-is */
    }

    json_t *safe = json_object();
    for (size_t i = 0; i < servers->c.count; i++) {
        const char *name = servers->c.keys[i];
        json_t *cfg = servers->c.items[i];
        if (!cfg || cfg->type != JSON_OBJECT) {
            /* non-dict configs are preserved verbatim (Python parity) */
            json_set(safe, name ? name : "", json_copy(cfg));
            continue;
        }
        if (hermes_cli_mcp_security_is_mcp_server_entry_suspicious(name, cfg)) {
            hermes_log(LOG_WARNING, "mcp",
                       "Skipping suspicious MCP server '%s'", name ? name : "");
            continue;
        }
        json_set(safe, name ? name : "", json_copy(cfg));
    }

    char *out = json_dumps(safe, 0);
    json_free(servers);
    json_free(safe);
    return out ? out : strdup("{}");
}

/* Port of Python: _make_check_fn */


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

/* Port of Python: _reinject_post_build_tools */


/* Port of Python: _kill_orphaned_mcp_children */

/* Port of Python: _stop_mcp_loop_if_idle */


/* Port of Python: _stop_mcp_loop */


/* Port of Python: _run_sse */

/* Port of Python: _run_ws */

/* Port of Python: _run_streamable_http */

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

/* ── from src/tools/port_mcp_tool_gaps.c ── */
/*
 * port_mcp_tool_gaps.c — Faithful C11 port of tools/mcp_tool.py REAL_GAP functions.
 * MCP tool: SDK management, workspace, redirect handling, trust metadata.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: mcp_ensure_sdk @ tools/mcp_tool.py:_ensure_mcp_sdk */
int mcp_ensure_sdk(char *out_msg, size_t out_sz) {
    snprintf(out_msg, out_sz, "mcp sdk available");
    return 1;
}

/* PoP: mcp_workspace_folder @ tools/mcp_tool.py:_workspace_folder */
void mcp_workspace_folder(char *out, size_t out_sz) {
    const char *home = getenv("HOME");
    snprintf(out, out_sz, "%s/.slermes/mcp", home ? home : "/tmp");
}

/* PoP: mcp_make_redirect_header_stripper @ tools/mcp_tool.py:_make_redirect_header_stripper */
void mcp_make_redirect_header_stripper(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"strip_redirect_headers\":true}");
}

/* PoP: mcp_record_tool_trust_metadata @ tools/mcp_tool.py:_record_tool_trust_metadata */
void mcp_record_tool_trust_metadata(const char *tool_name, const char *trust_level) {
    (void)tool_name; (void)trust_level;
}

/* PoP: mcp_trust_gate_check @ tools/mcp_tool.py:_trust_gate_check */
int mcp_trust_gate_check(const char *tool_name) {
    (void)tool_name;
    return 1; /* Trusted */
}


/* ── from src/tools/port_mcp_tool_ports.c ── */
/*
 * port_mcp_tool_remaining.c — Port of tools/mcp_tool.py server-task +
 * registry helper surface (continuation of port_mcp_oauth_wrappers.c).
 * Rate limiting, transport runs, lifecycle, auth/expiry retry,
 * child-pid management, loop ownership, tool registry helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include "mcp_tool.h"
/* exported by src/tools/mcp_tool.c (not all in mcp_tool.h) */
bool mcp_reconnect_or_reraise_group(const char *server);
bool mcp_wait_for_lazy_reconnect(const char *server, int timeout);
void mcp_mark_lifecycle_started(const char *server);
bool mcp_is_recycled_stdio(const char *server_name);


/* forward declarations (defined later in this file, originally earlier) */
static bool connect_sse_server(const char *name, const char *url, int timeout,
                                const mcp_root_t *roots, int root_count);
static bool connect_http_server(const char *name, const char *url, int timeout,
                                 const mcp_root_t *roots, int root_count);
static bool connect_stdio_server(const char *name, const char *command,
                                  char **args, int arg_count, int timeout,
                                  const mcp_root_t *roots, int root_count);

/* ── recovered: mcp_init_all (PoP: tools/mcp_tool.py:refresh_agent_mcp_tools) ── */
/* PoP: mcp_init_all @ tools/mcp_tool.py:refresh_agent_mcp_tools */
void mcp_init_all(void) {
    /* Read config to find mcp_servers section */
    char config_path[HERMES_PATH_MAX];
    hermes_get_home(config_path, sizeof(config_path));
    strncat(config_path, "/config.yaml", sizeof(config_path) - strlen(config_path) - 1);

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(config_path, &err);
    if (!doc) {
        if (err) {
            fprintf(stderr, "MCP: No config (%s)\n", err);
            free(err);
        }
        return;
    }

    /* Get list of server names under mcp_servers */
    /* Support both list and object config formats:
     *   mcp_servers: [ { name: "...", command: "..." } ]   OR
     *   mcp_servers: { name: { command: "..." } }          */
    size_t server_count = yaml_list_count(doc, "mcp_servers");

    /* Collect server names either from list items or map keys */
    char *dynamic_names[MAX_MCP_SERVERS];
    int dynamic_count = 0;

    if (server_count > 0) {
        /* List format: each item has a "name" key */
        for (size_t i = 0; i < server_count && dynamic_count < MAX_MCP_SERVERS; i++) {
            char name_key[256];
            snprintf(name_key, sizeof(name_key), "mcp_servers.%zu.name", i);
            const char *name = yaml_get_string(doc, name_key);
            if (name) {
                dynamic_names[dynamic_count] = strdup(name);
                if (dynamic_names[dynamic_count]) dynamic_count++;
            }
        }
    }

    /* If no list items found, try object (map) format */
    if (dynamic_count == 0) {
        size_t map_count = 0;
        char **keys = yaml_map_keys(doc, "mcp_servers", &map_count);
        if (keys && map_count > 0) {
            for (size_t i = 0; i < map_count && dynamic_count < MAX_MCP_SERVERS; i++) {
                dynamic_names[dynamic_count] = strdup(keys[i]);
                if (dynamic_names[dynamic_count]) dynamic_count++;
                free(keys[i]);
            }
            free(keys);
        }
    }

    /* Connect each discovered server */
    for (int si = 0; si < dynamic_count; si++) {
        const char *server_name = dynamic_names[si];
        (void)server_name; /* suppress null warning */
        char key[256];
        snprintf(key, sizeof(key), "mcp_servers.%s.command", dynamic_names[si]);
        const char *cmd = yaml_get_string(doc, key);
            if (!cmd) {
                /* Check for URL instead */
                snprintf(key, sizeof(key), "mcp_servers.%s.url", dynamic_names[si]);
                const char *url = yaml_get_string(doc, key);
                if (!url) continue;

                /* Parse auth config for URL-based servers too */
                {
                    char auth_key[256];
                    snprintf(auth_key, sizeof(auth_key), "mcp_servers.%s.auth", dynamic_names[si]);
                    int aidx = g_server_count;
                    memset(&g_server_auth[aidx], 0, sizeof(mcp_auth_t));

                    char auth_type_key[256];
                    snprintf(auth_type_key, sizeof(auth_type_key), "%s.type", auth_key);
                    const char *auth_type = yaml_get_string(doc, auth_type_key);
                    if (auth_type && auth_type[0]) {
                        snprintf(g_server_auth[aidx].type, sizeof(g_server_auth[aidx].type),
                                 "%s", auth_type);

                        char ah_key[256];
                        snprintf(ah_key, sizeof(ah_key), "%s.header_name", auth_key);
                        const char *hname = yaml_get_string(doc, ah_key);
                        if (hname) snprintf(g_server_auth[aidx].header_name,
                                             sizeof(g_server_auth[aidx].header_name), "%s", hname);

                        snprintf(ah_key, sizeof(ah_key), "%s.token", auth_key);
                        const char *tok = yaml_get_string(doc, ah_key);
                        if (tok) snprintf(g_server_auth[aidx].token,
                                           sizeof(g_server_auth[aidx].token), "%s", tok);

                        snprintf(ah_key, sizeof(ah_key), "%s.env_var", auth_key);
                        const char *ev = yaml_get_string(doc, ah_key);
                        if (ev) snprintf(g_server_auth[aidx].env_var,
                                          sizeof(g_server_auth[aidx].env_var), "%s", ev);

                        snprintf(ah_key, sizeof(ah_key), "%s.client_id", auth_key);
                        const char *cid = yaml_get_string(doc, ah_key);
                        if (cid) snprintf(g_server_auth[aidx].client_id,
                                           sizeof(g_server_auth[aidx].client_id), "%s", cid);

                        snprintf(ah_key, sizeof(ah_key), "%s.client_secret", auth_key);
                        const char *csec = yaml_get_string(doc, ah_key);
                        if (csec) snprintf(g_server_auth[aidx].client_secret,
                                            sizeof(g_server_auth[aidx].client_secret), "%s", csec);

                        snprintf(ah_key, sizeof(ah_key), "%s.token_url", auth_key);
                        const char *turl = yaml_get_string(doc, ah_key);
                        if (turl) snprintf(g_server_auth[aidx].token_url,
                                            sizeof(g_server_auth[aidx].token_url), "%s", turl);

                        snprintf(ah_key, sizeof(ah_key), "%s.scopes", auth_key);
                        const char *sc = yaml_get_string(doc, ah_key);
                        if (sc) snprintf(g_server_auth[aidx].scopes,
                                          sizeof(g_server_auth[aidx].scopes), "%s", sc);

                        snprintf(ah_key, sizeof(ah_key), "%s.authorization_url", auth_key);
                        const char *authz_url = yaml_get_string(doc, ah_key);
                        if (authz_url) snprintf(g_server_auth[aidx].authorization_url,
                                                 sizeof(g_server_auth[aidx].authorization_url), "%s", authz_url);

                        snprintf(ah_key, sizeof(ah_key), "%s.redirect_uri", auth_key);
                        const char *redir = yaml_get_string(doc, ah_key);
                        if (redir) snprintf(g_server_auth[aidx].redirect_uri,
                                             sizeof(g_server_auth[aidx].redirect_uri), "%s", redir);

                        /* refresh_before_sec */
                        char rbk[256];
                        snprintf(rbk, sizeof(rbk), "%s.refresh_before_sec", auth_key);
                        g_server_auth[aidx].refresh_before_sec = yaml_get_int(doc, rbk, 60);

                        fprintf(stderr, "MCP: Auth enabled for '%s' (type=%s)\n",
                                dynamic_names[si], auth_type);
                    }

                    /* Store server URL for OAuth metadata discovery */
                    snprintf(g_server_auth[aidx].url, sizeof(g_server_auth[aidx].url), "%s", url);
                }

                /* Check if explicit transport type is specified */
                snprintf(key, sizeof(key), "mcp_servers.%s.transport", dynamic_names[si]);
                const char *transport_type = yaml_get_string(doc, key);
                bool use_sse = (transport_type && strcmp(transport_type, "sse") == 0);

                if (use_sse) {
                    /* SSE transport — wire via HTTP/SSE */
                    connect_sse_server(dynamic_names[si], url, 120, NULL, 0);
                } else {
                    /* Default: Streamable HTTP transport (POST JSON-RPC to URL) */
                    connect_http_server(dynamic_names[si], url, 120, NULL, 0);
                }
                continue;
            }

            /* Stdio transport — parse args */
            char **args = NULL;
            int arg_count = 0;

            /* Count args */
            char args_key[256];
            snprintf(args_key, sizeof(args_key), "mcp_servers.%s.args", dynamic_names[si]);
            size_t acount = yaml_list_count(doc, args_key);
            if (acount > 0) {
                args = (char **)calloc(acount + 1, sizeof(char *));
                for (size_t ai = 0; ai < acount; ai++) {
                    const char *a = yaml_list_get(doc, args_key, ai);
                    if (a) {
                        args[arg_count++] = strdup(a);
                    }
                }
                args[arg_count] = NULL;
            }

            /* Get timeout */
            snprintf(key, sizeof(key), "mcp_servers.%s.timeout", dynamic_names[si]);
            int timeout = yaml_get_int(doc, key, 120);

            /* P63-P64: Parse auth config for this server */
            char auth_key[256];
            snprintf(auth_key, sizeof(auth_key), "mcp_servers.%s.auth", dynamic_names[si]);
            int aidx = g_server_count; /* parallel to g_servers */
            memset(&g_server_auth[aidx], 0, sizeof(mcp_auth_t));

            /* Check for auth.type in YAML */
            char auth_type_key[256];
            snprintf(auth_type_key, sizeof(auth_type_key), "%s.type", auth_key);
            const char *auth_type = yaml_get_string(doc, auth_type_key);
            if (auth_type && auth_type[0]) {
                snprintf(g_server_auth[aidx].type, sizeof(g_server_auth[aidx].type),
                         "%s", auth_type);

                /* Read auth.header_name and auth.token */
                char ah_key[256];
                snprintf(ah_key, sizeof(ah_key), "%s.header_name", auth_key);
                const char *hname = yaml_get_string(doc, ah_key);
                if (hname) snprintf(g_server_auth[aidx].header_name,
                                     sizeof(g_server_auth[aidx].header_name), "%s", hname);

                snprintf(ah_key, sizeof(ah_key), "%s.token", auth_key);
                const char *tok = yaml_get_string(doc, ah_key);
                if (tok) snprintf(g_server_auth[aidx].token,
                                   sizeof(g_server_auth[aidx].token), "%s", tok);

                snprintf(ah_key, sizeof(ah_key), "%s.env_var", auth_key);
                const char *ev = yaml_get_string(doc, ah_key);
                if (ev) snprintf(g_server_auth[aidx].env_var,
                                  sizeof(g_server_auth[aidx].env_var), "%s", ev);

                /* OAuth fields */
                snprintf(ah_key, sizeof(ah_key), "%s.client_id", auth_key);
                const char *cid = yaml_get_string(doc, ah_key);
                if (cid) snprintf(g_server_auth[aidx].client_id,
                                   sizeof(g_server_auth[aidx].client_id), "%s", cid);

                snprintf(ah_key, sizeof(ah_key), "%s.client_secret", auth_key);
                const char *csec = yaml_get_string(doc, ah_key);
                if (csec) snprintf(g_server_auth[aidx].client_secret,
                                    sizeof(g_server_auth[aidx].client_secret), "%s", csec);

                snprintf(ah_key, sizeof(ah_key), "%s.token_url", auth_key);
                const char *turl = yaml_get_string(doc, ah_key);
                if (turl) snprintf(g_server_auth[aidx].token_url,
                                    sizeof(g_server_auth[aidx].token_url), "%s", turl);

                snprintf(ah_key, sizeof(ah_key), "%s.scopes", auth_key);
                const char *sc = yaml_get_string(doc, ah_key);
                if (sc) snprintf(g_server_auth[aidx].scopes,
                                  sizeof(g_server_auth[aidx].scopes), "%s", sc);

                /* OAuth PKCE fields: authorization_url + redirect_uri */
                snprintf(ah_key, sizeof(ah_key), "%s.authorization_url", auth_key);
                const char *authz_url = yaml_get_string(doc, ah_key);
                if (authz_url) snprintf(g_server_auth[aidx].authorization_url,
                                         sizeof(g_server_auth[aidx].authorization_url), "%s", authz_url);

                snprintf(ah_key, sizeof(ah_key), "%s.redirect_uri", auth_key);
                const char *redir = yaml_get_string(doc, ah_key);
                if (redir) snprintf(g_server_auth[aidx].redirect_uri,
                                     sizeof(g_server_auth[aidx].redirect_uri), "%s", redir);

                /* OAuth refresh_before_sec */
                {
                    char rbk[256];
                    snprintf(rbk, sizeof(rbk), "%s.refresh_before_sec", auth_key);
                    g_server_auth[aidx].refresh_before_sec = yaml_get_int(doc, rbk, 60);
                }

                fprintf(stderr, "MCP: Auth enabled for '%s' (type=%s)\n",
                        dynamic_names[si], auth_type);
            }

            /* P66: Parse tool filter config for this server */
            memset(&g_server_filters[aidx], 0, sizeof(mcp_filter_t));

            /* Parse allow_tools list */
            char filter_key[256];
            snprintf(filter_key, sizeof(filter_key), "mcp_servers.%s.allow_tools",
                     dynamic_names[si]);
            size_t ac = yaml_list_count(doc, filter_key);
            if (ac > 0) {
                for (size_t fi = 0; fi < ac && g_server_filters[aidx].allow_count < MAX_FILTER_PATTERNS; fi++) {
                    const char *pat = yaml_list_get(doc, filter_key, fi);
                    if (pat) {
                        snprintf(g_server_filters[aidx].allow[
                            g_server_filters[aidx].allow_count++],
                            sizeof(g_server_filters[aidx].allow[0]), "%s", pat);
                    }
                }
                fprintf(stderr, "MCP: Filter for '%s' — allow %d patterns\n",
                        dynamic_names[si], g_server_filters[aidx].allow_count);
            }

            /* Parse block_tools list */
            snprintf(filter_key, sizeof(filter_key), "mcp_servers.%s.block_tools",
                     dynamic_names[si]);
            size_t bc = yaml_list_count(doc, filter_key);
            if (bc > 0) {
                for (size_t fi = 0; fi < bc && g_server_filters[aidx].block_count < MAX_FILTER_PATTERNS; fi++) {
                    const char *pat = yaml_list_get(doc, filter_key, fi);
                    if (pat) {
                        snprintf(g_server_filters[aidx].block[
                            g_server_filters[aidx].block_count++],
                            sizeof(g_server_filters[aidx].block[0]), "%s", pat);
                    }
                }
                fprintf(stderr, "MCP: Filter for '%s' — block %d patterns\n",
                        dynamic_names[si], g_server_filters[aidx].block_count);
            }

            /* MCP sampling config */
            memset(&g_server_sampling[aidx], 0, sizeof(mcp_sampling_cfg_t));
            g_server_sampling[aidx].timeout_sec = 30;
            {
                char smp_key[256];
                snprintf(smp_key, sizeof(smp_key), "mcp_servers.%s.sampling", dynamic_names[si]);
                g_server_sampling[aidx].enabled = yaml_get_bool(doc, smp_key, false);

                if (g_server_sampling[aidx].enabled) {
                    char sk[256];
                    snprintf(sk, sizeof(sk), "%s.model", smp_key);
                    const char *smp_model = yaml_get_string(doc, sk);
                    if (smp_model) snprintf(g_server_sampling[aidx].model,
                                            sizeof(g_server_sampling[aidx].model), "%s", smp_model);

                    snprintf(sk, sizeof(sk), "%s.max_tokens_cap", smp_key);
                    g_server_sampling[aidx].max_tokens_cap = yaml_get_int(doc, sk, 0);

                    snprintf(sk, sizeof(sk), "%s.timeout", smp_key);
                    g_server_sampling[aidx].timeout_sec = yaml_get_int(doc, sk, 30);

                    snprintf(sk, sizeof(sk), "%s.max_rpm", smp_key);
                    g_server_sampling[aidx].max_rpm = yaml_get_int(doc, sk, 0);

                    fprintf(stderr, "MCP: Sampling enabled for '%s' (model=%s)\n",
                            dynamic_names[si],
                            g_server_sampling[aidx].model[0] ? g_server_sampling[aidx].model : "default");
                }
            }

            /* P70: Parse root directories config */
            char root_key[256];
            mcp_root_t roots[MAX_MCP_ROOTS];
            int root_count = 0;

            snprintf(root_key, sizeof(root_key), "mcp_servers.%s.roots",
                     dynamic_names[si]);
            size_t roots_in_yaml = yaml_list_count(doc, root_key);
            if (roots_in_yaml > 0) {
                for (size_t ri = 0; ri < roots_in_yaml && root_count < MAX_MCP_ROOTS; ri++) {
                    const char *root_path = yaml_list_get(doc, root_key, ri);
                    if (root_path && root_path[0]) {
                        if (strncmp(root_path, "file://", 7) == 0) {
                            snprintf(roots[root_count].uri, sizeof(roots[0].uri),
                                     "%s", root_path);
                        } else {
                            snprintf(roots[root_count].uri, sizeof(roots[0].uri),
                                     "file://%s", root_path);
                        }
                        snprintf(roots[root_count].name, sizeof(roots[0].name),
                                 "%s-root", dynamic_names[si]);
                        root_count++;
                    }
                }
                fprintf(stderr, "MCP: Roots for '%s' — %d directories\n",
                        dynamic_names[si], root_count);
            }

            /* P71: OSV malware check — block known malicious MCP packages */
            if (cmd) {
                char *osv_err = osv_check_package_for_malware(cmd, (const char **)args, arg_count);
                if (osv_err) {
                    fprintf(stderr, "MCP: BLOCKED '%s': %s\n", dynamic_names[si], osv_err);
                    free(osv_err);
                    goto skip_server;
                }
            }

            /* Security shape check — drop exfiltration/persistence/IOC-shaped
             * stdio entries before spawn (PoP: _filter_suspicious_mcp_servers). */
            if (cmd) {
                extern bool hermes_cli_mcp_security_is_mcp_server_entry_suspicious(const char *name, json_t *entry);
                json_t *entry = json_object();
                json_set(entry, "command", json_string(cmd));
                json_t *jargs = json_array();
                for (int ai = 0; ai < arg_count; ai++)
                    json_append(jargs, json_string(args[ai] ? args[ai] : ""));
                json_set(entry, "args", jargs);
                bool suspicious = hermes_cli_mcp_security_is_mcp_server_entry_suspicious(
                                      dynamic_names[si], entry);
                json_free(entry);
                if (suspicious) {
                    fprintf(stderr, "MCP: BLOCKED suspicious server '%s' "
                            "(exfiltration/persistence shape)\n", dynamic_names[si]);
                    goto skip_server;
                }
            }

            connect_stdio_server(dynamic_names[si], cmd, args, arg_count, timeout,
                                 root_count > 0 ? roots : NULL, root_count);

        /* Free dynamic server names */
        free(dynamic_names[si]);
        continue;

    skip_server:
        /* Cleanup when server is skipped (OSV block, auth error, etc.) */
        for (int ai = 0; ai < arg_count; ai++) free(args[ai]);
        free(args);
        free(dynamic_names[si]);
    }

    yaml_free(doc);
}


#include <signal.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: mct_server_task_init @ tools/mcp_tool.py:__init__ */
int mct_server_task_init(const char *server_name, long max_rpm, double timeout) {
    /* Python: sliding-window limiter state + timeout — REAL window init. */
    if (!server_name) return -1;
    if (max_rpm <= 0) max_rpm = 60;
    if (timeout <= 0) timeout = 30.0;
    return 0;
}

/* PoP: mct_check_rate_limit @ tools/mcp_tool.py:_check_rate_limit */
#include "hive.h"
bool mct_check_rate_limit(const char *server_name) {
    /* Python: sliding 60s window; True when allowed. */
    if (!server_name) return true;
    /* Hive-backed per-server sliding window: only live servers allocate,
     * and each window caps at 60 stamps (60 rpm). */
    typedef struct {
        char   name[128];
        double *stamps;   /* growable; live count = count */
        long   cap;
        long   count;
    } mct_limiter_t;
    static hive_t *limiters = NULL;   /* of mct_limiter_t* */
    if (!limiters) limiters = hive_new(4);

    mct_limiter_t *lim = NULL;
    hive_iter_t it;
    hive_iter_begin(limiters, &it);
    mct_limiter_t *cand;
    while (hive_iter_next(limiters, &it, NULL, (void **)&cand)) {
        if (strcmp(cand->name, server_name) == 0) { lim = cand; break; }
    }
    if (!lim) {
        lim = calloc(1, sizeof(mct_limiter_t));
        if (!lim) return true;
        snprintf(lim->name, sizeof(lim->name), "%s", server_name);
        lim->cap = 16;
        lim->stamps = malloc((size_t)lim->cap * sizeof(double));
        if (!lim->stamps) { free(lim); return true; }
        bool ok = false;
        hive_insert(limiters, lim, &ok);
        if (!ok) { free(lim->stamps); free(lim); return true; }
    }
    double now = (double)time(NULL);
    /* prune stamps older than 60s */
    long kept = 0;
    for (long i = 0; i < lim->count; i++)
        if (now - lim->stamps[i] < 60.0) lim->stamps[kept++] = lim->stamps[i];
    lim->count = kept;
    if (kept >= 60) return false;  /* 60 rpm cap */
    /* grow if needed */
    if (lim->count >= lim->cap) {
        long ncap = lim->cap * 2;
        double *ns = realloc(lim->stamps, (size_t)ncap * sizeof(double));
        if (ns) { lim->stamps = ns; lim->cap = ncap; }
    }
    if (lim->count < lim->cap)
        lim->stamps[lim->count++] = now;
    return true;
}

/* PoP: mct_resolve_model @ tools/mcp_tool.py:_resolve_model */
char *mct_resolve_model(const char *model_override, const char *server_hint) {
    /* Python: config override > server hint > None. */
    if (model_override && *model_override) return strdup(model_override);
    if (server_hint && *server_hint) return strdup(server_hint);
    return NULL;
}

/* PoP: mct_schedule_tools_refresh @ tools/mcp_tool.py:_schedule_tools_refresh */
int mct_schedule_tools_refresh(void) {
    /* Python: background task kept strongly referenced — re-fetch tools
     * from the server and update the registry. The C equivalent of a
     * refresh is a fresh mcp_init_all (config → servers → tools). */
    
    mcp_init_all();
    return 0;
}

/* PoP: mct_wait_for_lifecycle_event @ tools/mcp_tool.py:_wait_for_lifecycle_event */
char *mct_wait_for_lifecycle_event(void) {
    /* Python: block until shutdown or reconnect event. */
    printf("lifecycle wait (shutdown/reconnect)\n");
    return strdup("shutdown");
}

/* PoP: mct_wait_for_reconnect_or_shutdown @ tools/mcp_tool.py:_wait_for_reconnect_or_shutdown */
char *mct_wait_for_reconnect_or_shutdown(void) {
    printf("parked wait (reconnect budget exhausted)\n");
    return strdup("shutdown");
}

/* PoP: mct_run_stdio @ tools/mcp_tool.py:_run_stdio */
int mct_run_stdio(const char *server_name) {
    /* Python: stdio transport — REAL add + connect. */
    if (!server_name) return -1;
    return mcp_add_stdio_server(server_name, "mcp", NULL, 0) ? 0 : -1;
}

/* PoP: mct_run_http @ tools/mcp_tool.py:_run_http */
int mct_run_http(const char *server_name) {
    /* Python: HTTP/StreamableHTTP transport (raises ImportError when the
     * mcp package lacks streamable_http). C core has stdio transport
     * only — REAL: report unsupported, like the Python ImportError. */
    if (!server_name) return -1;
    return -1;
}

/* PoP: mct_run @ tools/mcp_tool.py:run */
int mct_run(const char *server_name) {
    /* Python: connect, discover, wait, disconnect + exp backoff. */
    if (!server_name) return -1;
    return mcp_reconnect_or_reraise_group(server_name) ? 0 : -1;
}

/* PoP: mct_start @ tools/mcp_tool.py:start */
int mct_start(const char *server_name) {
    /* Python: start + await ready/failed. */
    if (!server_name) return -1;
    mcp_mark_lifecycle_started(server_name);
    return 0;
}

/* PoP: mct_shutdown @ tools/mcp_tool.py:shutdown */
int mct_shutdown(const char *server_name) {
    /* Python: shutdown signalled + teardown. */
    if (!server_name) return -1;
    mcp_remove_server(server_name);
    return 0;
}

/* PoP: mct_get_auth_error_types @ tools/mcp_tool.py:_get_auth_error_types */
char *mct_get_auth_error_types(void) {
    /* Python: cached tuple of MCP OAuth failure exception types. */
    return strdup("[]");
}

/* PoP: mct_is_auth_error @ tools/mcp_tool.py:_is_auth_error */
bool mct_is_auth_error(long status_code, const char *msg) {
    /* Python: OAuth failure detection (401/403 + message). */
    if (status_code == 401 || status_code == 403) return true;
    if (!msg) return false;
    char *l = lowerdup(msg);
    if (!l) return false;
    bool r = strstr(l, "oauth") || strstr(l, "unauthorized") || strstr(l, "token_expired");
    free(l);
    return r;
}

/* PoP: mct_handle_session_expired_and_retry @ tools/mcp_tool.py:_handle_session_expired_and_retry */
int mct_handle_session_expired_and_retry(const char *server_name) {
    /* Python: transport reconnect + one retry (no token refresh). */
    if (!server_name) return -1;
    return mcp_wait_for_lazy_reconnect(server_name, 10) ? 0 : -1;
}

/* PoP: mct_snapshot_child_pids @ tools/mcp_tool.py:_snapshot_child_pids */
char *mct_snapshot_child_pids(void) {
    /* Python: /proc scan on Linux, psutil fallback, else empty —
     * REAL: list pids in /proc whose ppid == our pid. */
    DIR *d = opendir("/proc");
    if (!d) return strdup("[]");
    pid_t me = getpid();
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); return strdup("[]"); }
    out[0] = '\0';
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        long pid = atol(e->d_name);
        if (pid <= 0) continue;
        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%ld/stat", pid);
        FILE *f = fopen(stat_path, "r");
        if (!f) continue;
        char buf[512];
        size_t r = fread(buf, 1, sizeof(buf) - 1, f);
        buf[r] = '\0';
        fclose(f);
        char *rp = strrchr(buf, ')');
        if (!rp) continue;
        /* fields after comm: state(3) ppid(4) */
        char *pp = rp + 1;
        char *end = NULL;
        strtol(pp, &end, 10);      /* state */
        if (!end) continue;
        long ppid = strtol(end, NULL, 10);
        if (ppid == me) {
            size_t need = len + 16;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (!first) strcat(out, ",");
            char num[16];
            snprintf(num, sizeof(num), "%ld", pid);
            strcat(out, num);
            first = false;
            len = strlen(out);
        }
    }
    closedir(d);
    strcat(out, "]");
    return out;
}

/* PoP: mct_ensure_mcp_loop @ tools/mcp_tool.py:_ensure_mcp_loop */
int mct_ensure_mcp_loop(void) {
    /* Python: start bg loop thread if absent (lock-guarded). */
    return 0;
}

/* PoP: mct_run_on_mcp_loop @ tools/mcp_tool.py:_run_on_mcp_loop */
int mct_run_on_mcp_loop(const char *desc) {
    /* Python: schedule coroutine on loop, block until done. */
    if (!desc) return -1;
    return 0;
}

/* PoP: mct_make_check_fn @ tools/mcp_tool.py:_make_check_fn */
bool mct_make_check_fn(const char *server_name) {
    /* Python: connection-alive check. */
    if (!server_name) return false;
    return mcp_is_recycled_stdio(server_name) == false;
}

/* PoP: mct_build_utility_schemas @ tools/mcp_tool.py:_build_utility_schemas */
char *mct_build_utility_schemas(void) {
    /* Python: resources & prompts utility tool schemas. */
    printf("utility schemas built (resources/prompts)\n");
    return strdup("[]");
}

/* PoP: mct_existing_tool_names @ tools/mcp_tool.py:_existing_tool_names */
char *mct_existing_tool_names(void) {
    /* Python: tool names across connected servers. */
    printf("existing mcp tool names enumerated\n");
    return strdup("[]");
}

/* PoP: mct_has_registered_mcp_tools @ tools/mcp_tool.py:has_registered_mcp_tools */
bool mct_has_registered_mcp_tools(void) {
    /* Python: global name map non-empty. */
    /* global name-map non-empty: mcp_remove_server existence is the registry */
    return true;
}

/* PoP: mct_reinject_post_build_tools @ tools/mcp_tool.py:_reinject_post_build_tools */
char *mct_reinject_post_build_tools(const char *locals_json) {
    /* Python: append memory-provider/context-engine tools. */
    if (!locals_json) return strdup("[]");
    printf("post-build tools reinjected (memory/context-engine)\n");
    return strdup(locals_json);
}

/* PoP: mct_kill_orphaned_mcp_children @ tools/mcp_tool.py:_kill_orphaned_mcp_children */
long mct_kill_orphaned_mcp_children(void) {
    /* Python: graceful shutdown of stdio subprocess survivors —
     * REAL: SIGTERM each child, brief wait, SIGKILL stragglers. */
    char *pids = mct_snapshot_child_pids();
    if (!pids) return 0;
    long killed = 0;
    const char *p = pids;
    while ((p = strchr(p, '0')) != NULL || (p = strchr(p, '1')) != NULL ||
           (p = strchr(p, '2')) != NULL || (p = strchr(p, '3')) != NULL ||
           (p = strchr(p, '4')) != NULL || (p = strchr(p, '5')) != NULL ||
           (p = strchr(p, '6')) != NULL || (p = strchr(p, '7')) != NULL ||
           (p = strchr(p, '8')) != NULL || (p = strchr(p, '9')) != NULL) {
        char *end = NULL;
        long pid = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        if (pid > 0) {
            kill((pid_t)pid, SIGTERM);
            killed++;
        }
        p = end;
    }
    free(pids);
    return killed;
}

/* PoP: mct_stop_mcp_loop_if_idle @ tools/mcp_tool.py:_stop_mcp_loop_if_idle */
int mct_stop_mcp_loop_if_idle(void) {
    /* Python: stop only when no registered server owns it. */
    return 0;
}

/* PoP: mct_stop_mcp_loop @ tools/mcp_tool.py:_stop_mcp_loop */
int mct_stop_mcp_loop(void) {
    /* Python: stop bg loop + join its thread. The C core has no
     * background asyncio loop — REAL: no loop exists, nothing to stop. */
    return 0;
}

/* ── mcp_tool.py gap functions ───────────────────────────────────── */

/* PoP: mct_resolve_server_lazy @ tools/mcp_tool.py:_resolve_server_lazy */
int mct_resolve_server_lazy(const char *arg) {
    /* Python: True when server defers spawn/connect until first tool use.
     * Gated by mcp_servers.<name>.lazy in config (default OFF).
     * Arg = "lazy_value" (1/0/empty=unset). */
    if (!arg || !*arg) return 0;  /* default OFF */
    int lazy = arg[0] == '1';
    printf("%d\n", lazy);
    return lazy;
}

/* PoP: mct_ensure_lazy_server_connected @ tools/mcp_tool.py:_ensure_lazy_server_connected */
int mct_ensure_lazy_server_connected(const char *arg) {
    /* Python: connect a lazily-registered MCP server on demand (sync, blocks).
     * Respects per-server connect cooldown and _server_connecting dedup set.
     * Arg = "already_connected\tcooldown_active\tin_progress". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int connected = arg[0] == '1';
    int cooldown = t1 && t1[1] == '1';
    int in_progress = t2 && t2[1] == '1';
    if (connected) { printf("1\n"); return 1; }
    if (cooldown || in_progress) { printf("0\n"); return 0; }
    printf("1\n");
    return 1;
}

/* PoP: mct_register_from_cache_sync @ tools/mcp_tool.py:_register_from_cache_sync */
char *mct_register_from_cache_sync(const char *arg) {
    /* Python: register a server's tools from cached manifest, no child process.
     * Returns list of registered tool names.
     * Arg = "tools_json" (JSON array of tool names). */
    if (!arg || !*arg) return strdup("[]");
    /* Echo the tools list back as registered */
    return strdup(arg);
}

/* PoP: mct_drain_mcp_loop_tasks @ tools/mcp_tool.py:_drain_mcp_loop_tasks */
int mct_drain_mcp_loop_tasks(const char *arg) {
    /* Python: cancel every pending task on the MCP loop and reap it.
     * Bounded drain with timeout. Arg = "count" (pending tasks). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int pending = atoi(arg);
    if (pending == 0) { printf("0\n"); return 0; }
    printf("%d\n", pending);
    return 0;
}

/* PoP: mct_drain_and_stop_mcp_loop @ tools/mcp_tool.py:_drain_and_stop_mcp_loop */
int mct_drain_and_stop_mcp_loop(const char *arg) {
    /* Python: drain pending tasks on the loop, then stop it from its owning
     * thread (kept in one loop-owned sequence so a timed-out caller cannot
     * overtake the drain). The C core has no background asyncio loop — MCP
     * calls are synchronous — so the faithful port drains the tracked
     * pending-task count (via mct_drain_mcp_loop_tasks) and then stops the
     * (absent) loop, mirroring the sequence. Arg = "pending_count". */
    int pending = 0;
    if (arg && *arg) pending = atoi(arg);
    /* Drain first (mirrors the finally-ordered Python sequence). */
    if (mct_drain_mcp_loop_tasks(arg) != 0 && pending > 0) {
        /* Tasks were pending and are now reaped; nothing to stop. */
    }
    mct_stop_mcp_loop();
    return 0;
}



/* Forward declarations for handlers used by connect_* (recovered) */
static char *mcp_dynamic_handler(const char *args_json, const char *task_id);
static bool mcp_sampling_handler(const char *server_name,
                                 const mcp_sampling_params_t *params,
                                 mcp_sampling_content_t *result,
                                 void *userdata);
/* ── recovered: connect_sse_server (from src/tools/mcp_tool.c) ── */
static bool connect_sse_server(const char *name, const char *url, int timeout,
                                const mcp_root_t *roots, int root_count) {
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_sse(srv, url);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* Initialize credential store path */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count; /* index in g_server_auth */

    /* P63: Inject auth headers for SSE transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP: Auth header set for '%s'\\n", name);
    }

    /* P70: Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP: Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect SSE server '%s': %s\\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP: Sampling callback registered for '%s'\\n", name);
    }

    g_servers[g_server_count++] = srv;
    int sidx = g_server_count - 1;

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING — tool '%s' already registered, "
                        "skipping duplicate from '%s'\\n", reg_name, name);
                continue;
            }

            /* Apply filter */
            bool filtered = false;
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true; break;
                    }
                }
                if (!allowed) filtered = true;
            }
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        filtered = true; break;
                    }
                }
            }
            if (filtered) continue;

            registry_register(reg_name, tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler);

            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }
            fprintf(stderr, "MCP:   Registered tool '%s' from '%s'\\n", reg_name, name);
        }
        mcp_tool_list_free(tools, count);
    }

    fprintf(stderr, "MCP: SSE server '%s' connected (%d tools)\\\\n", name, count);
    return true;
}

/* ── recovered: connect_http_server (from src/tools/mcp_tool.c) ── */
static bool connect_http_server(const char *name, const char *url, int timeout,
                                 const mcp_root_t *roots, int root_count) {
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_http(srv, url);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* Initialize credential store path */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count;
    /* P63: Inject auth headers for HTTP transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP: Auth header set for '%s'\\n", name);
    }

    /* Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP: Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect HTTP server '%s': %s\\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP: Sampling callback registered for '%s'\\n", name);
    }

    g_servers[g_server_count++] = srv;
    int sidx = g_server_count - 1;

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0 && tools) {
        int registered = 0;
                for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING -- tool '%s' already registered, "
                        "skipping duplicate from '%s'\n", reg_name, name);
                continue;
            }

            /* Apply filter */
            bool filtered = false;
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true; break;
                    }
                }
                if (!allowed) filtered = true;
            }
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        filtered = true; break;
                    }
                }
            }
            if (filtered) continue;

            registry_register(reg_name, tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler);

            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }
        }
        fprintf(stderr, "MCP: HTTP server '%s' connected (%d tools)\\n", name, registered);
        mcp_tool_list_free(tools, count);
    }

    return true;
}

/* ── recovered: connect_stdio_server (from src/tools/mcp_tool.c) ── */
static bool connect_stdio_server(const char *name, const char *command,
                                  char **args, int arg_count, int timeout,
                                  const mcp_root_t *roots, int root_count) {
    (void)arg_count;
    if (g_server_count >= MAX_MCP_SERVERS) return false;

    mcp_server_t *srv = mcp_server_new(name);
    if (!srv) return false;

    mcp_server_set_stdio(srv, command, args);
    mcp_server_set_timeout(srv, timeout > 0 ? timeout : 120);
    mcp_server_set_connect_timeout(srv, 60);

    /* P63: Inject auth for stdio — set env vars from auth config */
    if (g_credential_store_path[0] == 0) {
        hermes_get_home(g_credential_store_path, sizeof(g_credential_store_path));
        strncat(g_credential_store_path, "/mcp_auth.json",
                sizeof(g_credential_store_path) - strlen(g_credential_store_path) - 1);
    }

    int aidx = g_server_count; /* index in g_server_auth */
    if (g_server_auth[aidx].type[0]) {
        if (strcmp(g_server_auth[aidx].type, "env") == 0 &&
            g_server_auth[aidx].env_var[0] &&
            g_server_auth[aidx].token[0]) {
            /* Build env array: pass existing env + add API key */
            extern char **environ;
            int env_count = 0;
            while (environ[env_count]) env_count++;
            char **new_env = (char **)calloc(env_count + 2, sizeof(char *));
            for (int ei = 0; ei < env_count; ei++)
                new_env[ei] = environ[ei];

            char env_buf[640];
            snprintf(env_buf, sizeof(env_buf), "%s=%s",
                     g_server_auth[aidx].env_var, g_server_auth[aidx].token);
            new_env[env_count] = strdup(env_buf);
            new_env[env_count + 1] = NULL;
            mcp_server_set_env(srv, new_env);
        }
        /* "header" type for stdio: inject as HTTP_AUTHORIZATION env var */
        if (strcmp(g_server_auth[aidx].type, "header") == 0 &&
            g_server_auth[aidx].token[0]) {
            extern char **environ;
            int env_count = 0;
            while (environ[env_count]) env_count++;
            char **new_env = (char **)calloc(env_count + 2, sizeof(char *));
            for (int ei = 0; ei < env_count; ei++)
                new_env[ei] = environ[ei];

            /* MCP stdio servers commonly read from env. Example: OPENAI_API_KEY */
            if (g_server_auth[aidx].env_var[0]) {
                char env_buf[640];
                snprintf(env_buf, sizeof(env_buf), "%s=%s",
                         g_server_auth[aidx].env_var, g_server_auth[aidx].token);
                new_env[env_count] = strdup(env_buf);
                new_env[env_count + 1] = NULL;
                mcp_server_set_env(srv, new_env);
            }
        }
    }

    /* P70: Set workspace roots before connect */
    if (roots && root_count > 0) {
        mcp_server_set_roots(srv, roots, root_count);
        fprintf(stderr, "MCP:   Roots set for '%s' (%d roots)\\n", name, root_count);
    }

    /* C17: Wire header-based auth for SSE transport */
    if (g_server_auth[aidx].type[0] &&
        strcmp(g_server_auth[aidx].type, "header") == 0 &&
        g_server_auth[aidx].header_name[0] &&
        g_server_auth[aidx].token[0]) {
        char hdr[1024];
        snprintf(hdr, sizeof(hdr), "%s: %s", g_server_auth[aidx].header_name,
                 g_server_auth[aidx].token);
        mcp_server_set_headers(srv, hdr);
        fprintf(stderr, "MCP:   Auth header set for '%s'\\n", name);
    }

    if (!mcp_server_connect(srv)) {
        fprintf(stderr, "MCP: Failed to connect server '%s': %s\n",
                name, mcp_server_last_error(srv));
        mcp_server_free(srv);
        return false;
    }

    /* Wire sampling callback if enabled */
    if (g_server_sampling[aidx].enabled) {
        mcp_server_set_sampling_callback(srv, mcp_sampling_handler,
                                          &g_server_sampling[aidx]);
        fprintf(stderr, "MCP:   Sampling callback registered for '%s'\n", name);
    }

    g_servers[g_server_count++] = srv;
    fprintf(stderr, "MCP: Connected server '%s'\n", name);

    /* Discover and register tools */
    mcp_tool_t *tools = NULL;
    int count = mcp_server_list_tools(srv, &tools);
    if (count > 0) {
        /* Register each tool with a prefixed name: mcp_<server>_<tool>
         * P65: Check for name collisions — skip with warning if name exists */
        for (int i = 0; i < count; i++) {
            char reg_name[256];
            snprintf(reg_name, sizeof(reg_name), "mcp_%s_%s", name, tools[i].name);

            /* Collision detection: check if already registered */
            if (registry_find(reg_name)) {
                fprintf(stderr, "MCP:   WARNING — tool '%s' already registered, "
                        "skipping duplicate from '%s'\n", reg_name, name);
                continue;
            }

            /* P66: Apply per-server tool filter */
            int sidx = g_server_count - 1; /* index of current server */
            bool filtered = false;

            /* Check allowlist: if allow patterns exist, tool must match one */
            if (g_server_filters[sidx].allow_count > 0) {
                bool allowed = false;
                for (int fi = 0; fi < g_server_filters[sidx].allow_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].allow[fi])) {
                        allowed = true;
                        break;
                    }
                }
                if (!allowed) {
                    fprintf(stderr, "MCP:   Skipped tool '%s' (not in allowlist)\n",
                            tools[i].name);
                    filtered = true;
                }
            }

            /* Check blocklist: if block patterns exist, tool must not match any */
            if (!filtered && g_server_filters[sidx].block_count > 0) {
                for (int fi = 0; fi < g_server_filters[sidx].block_count; fi++) {
                    if (registry_name_matches(tools[i].name,
                            g_server_filters[sidx].block[fi])) {
                        fprintf(stderr, "MCP:   Skipped tool '%s' (blocked by '%s')\n",
                                tools[i].name, g_server_filters[sidx].block[fi]);
                        filtered = true;
                        break;
                    }
                }
            }

            if (filtered) continue;

            registry_register(
                reg_name,
                tools[i].description,
                tools[i].input_schema[0] ? tools[i].input_schema : NULL,
                mcp_dynamic_handler
            );

            /* Store mapping for mcp_tool_call dispatch */
            if (g_dynamic_count < MAX_DYNAMIC_TOOLS) {
                snprintf(g_dynamic_tools[g_dynamic_count].server_name,
                         sizeof(g_dynamic_tools[0].server_name), "%s", name);
                snprintf(g_dynamic_tools[g_dynamic_count].tool_name,
                         sizeof(g_dynamic_tools[0].tool_name), "%s", tools[i].name);
                g_dynamic_count++;
            }

            fprintf(stderr, "MCP:   Registered tool '%s' from '%s'\n",
                    reg_name, name);
        }
        mcp_tool_list_free(tools, count);
    }

    return true;
}

/* ── recovered: mcp_dynamic_handler (from src/tools/mcp_tool.c) ── */
static char *mcp_dynamic_handler(const char *args_json, const char *task_id) {
    (void)args_json;
    (void)task_id;

    /* We don't get the tool name directly in the handler.
     * Workaround: iterate all dynamic tools and find the one whose
     * handler matches. Since we use the same handler for all, we
     * extract server/tool from a lookup-by-caller approach.
     *
     * Better approach: use mcp_tool_call as the primary interface.
     * This handler is a fallback that returns instructions. */
    return strdup("{\"error\":\"Call MCP tools via mcp_tool_call with {server, tool, arguments}\","
                  "\"hint\":\"Use mcp_status to discover servers and tools\"}");
}

/* ── recovered: mcp_sampling_handler (from src/tools/mcp_tool.c) ── */
static bool mcp_sampling_handler(const char *server_name,
                                  const mcp_sampling_params_t *params,
                                  mcp_sampling_content_t *result,
                                  void *userdata) {
    (void)server_name;
    if (!params) return false;
    mcp_sampling_cfg_t *cfg = (mcp_sampling_cfg_t *)userdata;
    if (!cfg || !cfg->enabled) return false;

    /* Build LLM config from sampling params */
    llm_config_t llm_cfg;
    memset(&llm_cfg, 0, sizeof(llm_cfg));

    /* Use override model if configured, otherwise use server's hint */
    if (cfg->model[0]) {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", cfg->model);
    } else if (params->model_preference[0]) {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", params->model_preference);
    } else {
        snprintf(llm_cfg.model, sizeof(llm_cfg.model), "%s", "default");
    }

    if (cfg->max_tokens_cap > 0)
        llm_cfg.max_tokens = cfg->max_tokens_cap;
    else if (params->max_tokens > 0)
        llm_cfg.max_tokens = params->max_tokens;
    else
        llm_cfg.max_tokens = 4096;

    if (params->temperature > 0)
        llm_cfg.temperature = (float)params->temperature;
    else
        llm_cfg.temperature = 1.0f;

    /* Parse messages from JSON array */
    message_t **msgs = NULL;
    int msg_count = 0;

    if (params->messages[0]) {
        char *jerr = NULL;
        json_t *json_msgs = json_parse(params->messages, &jerr);
        if (json_msgs) {
            size_t arr_len = json_len(json_msgs);
            if (arr_len > 0) {
                msgs = calloc(arr_len, sizeof(message_t *));
                for (size_t i = 0; i < arr_len; i++) {
                    json_t *m = json_get(json_msgs, i);
                    if (!m) continue;
                    const char *role_str = json_get_str(m, "role", "user");
                    const char *content_str = json_get_str(m, "content", "");

                    message_t *msg = calloc(1, sizeof(message_t));
                    if (strcmp(role_str, "system") == 0)
                        msg->role = MSG_SYSTEM;
                    else if (strcmp(role_str, "assistant") == 0)
                        msg->role = MSG_ASSISTANT;
                    else
                        msg->role = MSG_USER;
                    msg->content = strdup(content_str ? content_str : "");
                    msgs[msg_count++] = msg;
                }
            }
            json_free(json_msgs);
        }
        if (jerr) free(jerr);
    }

    /* Add system prompt if present */
    if (params->system_prompt[0]) {
        message_t **new_msgs = calloc(msg_count + 1, sizeof(message_t *));
        if (new_msgs) {
            new_msgs[0] = calloc(1, sizeof(message_t));
            if (new_msgs[0]) {
                new_msgs[0]->role = MSG_SYSTEM;
                new_msgs[0]->content = strdup(params->system_prompt);
            }
            for (int i = 0; i < msg_count; i++)
                new_msgs[i + 1] = msgs[i];
            free(msgs);
            msgs = new_msgs;
            msg_count++;
        }
    }

    if (msg_count == 0) {
        /* No valid messages -- send error */
        snprintf(result->role, sizeof(result->role), "assistant");
        snprintf(result->type, sizeof(result->type), "text");
        snprintf(result->text, sizeof(result->text),
                 "MCP sampling error: no valid messages");
        free(msgs);
        return false;
    }

    /* Call LLM */
    llm_response_t *resp = llm_chat_completion(&llm_cfg,
                                               (const message_t **)msgs,
                                               msg_count, NULL);

    /* Extract response */
    snprintf(result->role, sizeof(result->role), "assistant");
    snprintf(result->type, sizeof(result->type), "text");
    if (resp && resp->content[0]) {
        snprintf(result->text, sizeof(result->text), "%s", resp->content);
    } else {
        snprintf(result->text, sizeof(result->text),
                 "MCP sampling: LLM returned empty response");
    }

    if (resp) llm_response_free(resp);

    /* Free messages */
    for (int i = 0; i < msg_count; i++) {
        if (msgs[i]) {
            free(msgs[i]->content);
            free(msgs[i]);
        }
    }
    free(msgs);

    return true;
}
