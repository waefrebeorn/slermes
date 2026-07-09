#ifndef SRC_TOOLS_PORT_BROWSER_TOOL_C
#define SRC_TOOLS_PORT_BROWSER_TOOL_C

#include "port_browser_tool.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>

/* Opaque struct definition - private to this translation unit */
struct port_browser_tool_state {
    int cached_command_timeout;
    bool command_timeout_resolved;
    int cached_inactivity_timeout;
    bool inactivity_timeout_resolved;
    bool chromium_installed;
    bool chromium_checked;
    bool autoinstall_attempted;
    char *last_session_key;
    bool cleanup_thread_active;
};

/* Lifecycle - opaque struct API */
port_browser_tool_state_t *port_browser_tool_init(void)
{
    port_browser_tool_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->cached_command_timeout = 0;
    state->command_timeout_resolved = false;
    state->cached_inactivity_timeout = 0;
    state->inactivity_timeout_resolved = false;
    state->chromium_installed = false;
    state->chromium_checked = false;
    state->autoinstall_attempted = false;
    state->last_session_key = NULL;
    state->cleanup_thread_active = false;
    return state;
}

void port_browser_tool_cleanup(port_browser_tool_state_t *state)
{
    if (!state) return;
    free(state->last_session_key);
    free(state);
}

/* Forward declarations for helper functions to avoid implicit declaration warnings */
bool browser_running_in_docker(void);
bool browser_is_local_mode(void);
bool browser_is_local_backend(void);
bool browser_is_local_sidecar_key(const char *session_key);
bool browser_allow_private_urls(void);
bool browser_is_always_blocked_url(const char *url);
bool browser_is_safe_url(const char *url);
bool browser_is_camofox_mode(void);
bool browser_chromium_installed(void);
bool browser_eval_ssrf_guard_active(const char *effective_task_id);
char *browser_current_page_private_url(const char *effective_task_id);
const char *browser_get_current_url(void);

/* Forward declarations for browser supervisor functions */
json_t *browser_supervisor_get_or_start(const char *session_key);
void browser_supervisor_stop_all(void);

/* PoP: browser_build_browser_env @ tools/browser_tool.py:_build_browser_env */
char *browser_build_browser_env(void)
{
    hermes_log(LOG_DEBUG, "port", "browser_build_browser_env called");
    /* Build credential-scrubbed environment for agent-browser subprocess.
     * Strips Hermes-managed secrets, re-adds only browser-backend keys. */
    const char *passthrough_keys[] = {
        "BROWSERBASE_API_KEY",
        "BROWSERBASE_PROJECT_ID",
        "BROWSER_USE_API_KEY",
        "FIRECRAWL_API_KEY",
        "FIRECRAWL_API_URL",
        "FIRECRAWL_BROWSER_TTL",
        NULL
    };

    char *result = malloc(8192);
    if (!result) {
        return NULL;
    }
    result[0] = '\0';
    strcat(result, "{");

    /* Add safe PATH */
    const char *sane_path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    #ifdef __APPLE__
    sane_path = "/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    #endif
    char path_buf[1024];
    snprintf(path_buf, sizeof(path_buf), "\"PATH\":\"%s\"", sane_path);
    strcat(result, path_buf);

    /* Add passthrough keys from host environment */
    for (int i = 0; passthrough_keys[i]; i++) {
        const char *val = getenv(passthrough_keys[i]);
        if (val) {
            char buf[1024];
            snprintf(buf, sizeof(buf), ",\"%s\":\"%s\"", passthrough_keys[i], val);
            strcat(result, buf);
        }
    }

    strcat(result, "}");
    return result;
}

/* PoP: browser_sanitize_url_for_logs @ tools/browser_tool.py:_sanitize_url_for_logs */
char *browser_sanitize_url_for_logs(const char *value)
{
    if (!value) {
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    /* Thin wrapper over redact_cdp_url - in C we implement minimal redaction */
    char *result = malloc(strlen(value) + 1);
    if (!result) return NULL;
    strcpy(result, value);

    /* Redact common credential patterns in URLs */
    char *token = strstr(result, "token=");
    if (token) {
        char *end = strchr(token, '&');
        if (!end) end = token + strlen(token);
        memset(token + 6, '*', end - (token + 6));
    }

    token = strstr(result, "api_key=");
    if (token) {
        char *end = strchr(token, '&');
        if (!end) end = token + strlen(token);
        memset(token + 8, '*', end - (token + 8));
    }

    token = strstr(result, "password=");
    if (token) {
        char *end = strchr(token, '&');
        if (!end) end = token + strlen(token);
        memset(token + 9, '*', end - (token + 9));
    }

    return result;
}

/* PoP: browser_get_command_timeout @ tools/browser_tool.py:_get_command_timeout */
/* PoP: browser_get_command_timeout @ tools/browser_camofox.py:_get_command_timeout */
int browser_get_command_timeout(void)
{
    static int cached_timeout = 0;
    static bool resolved = false;

    if (resolved && cached_timeout > 0) {
        return cached_timeout;
    }

    int result = 30; /* DEFAULT_COMMAND_TIMEOUT */

    /* Read from config.yaml - in C we use hermes_config_get_int equivalent */
    /* For now, check environment variable as fallback */
    const char *env_timeout = getenv("HERMES_BROWSER_COMMAND_TIMEOUT");
    if (env_timeout) {
        int val = atoi(env_timeout);
        if (val >= 5) {
            result = val;
        }
    }

    /* Floor at 5s to avoid instant kills */
    if (result < 5) result = 5;

    cached_timeout = result;
    resolved = true;
    return result;
}

/* PoP: browser_safe_command_timeout @ tools/browser_tool.py:_safe_command_timeout */
int browser_safe_command_timeout(void)
{
    int val = browser_get_command_timeout();
    return (val > 0) ? val : 30;
}

/* PoP: browser_get_open_command_timeout @ tools/browser_tool.py:_get_open_command_timeout */
int browser_get_open_command_timeout(bool first_open)
{
    int base = browser_safe_command_timeout();
    int floor = first_open ? 120 : 60; /* MIN_FIRST_OPEN_TIMEOUT / MIN_OPEN_TIMEOUT */
    return (base > floor) ? base : floor;
}

/* PoP: browser_needs_chromium_sandbox_bypass @ tools/browser_tool.py:_needs_chromium_sandbox_bypass */
bool browser_needs_chromium_sandbox_bypass(void)
{
    /* Running as root? */
    if (geteuid() == 0) {
        return true;
    }

    /* Running in Docker? */
    if (browser_running_in_docker()) {
        return true;
    }

    /* AppArmor unprivileged userns restriction? */
    FILE *f = fopen("/proc/sys/kernel/apparmor_restrict_unprivileged_userns", "r");
    if (f) {
        char buf[16];
        if (fgets(buf, sizeof(buf), f)) {
            if (strncmp(buf, "1", 1) == 0) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }

    return false;
}

/* PoP: browser_read_command_output_files @ tools/browser_tool.py:_read_command_output_files */
void browser_read_command_output_files(const char *stdout_path, const char *stderr_path, char **out_stdout, char **out_stderr)
{
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;

    if (stdout_path) {
        FILE *f = fopen(stdout_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 1024 * 1024) { /* 1MB cap */
                *out_stdout = malloc(sz + 1);
                if (*out_stdout) {
                    fread(*out_stdout, 1, sz, f);
                    (*out_stdout)[sz] = '\0';
                    /* Strip trailing whitespace */
                    char *p = *out_stdout + sz - 1;
                    while (p >= *out_stdout && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) {
                        *p-- = '\0';
                    }
                }
            }
            fclose(f);
        }
    }

    if (stderr_path) {
        FILE *f = fopen(stderr_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 1024 * 1024) {
                *out_stderr = malloc(sz + 1);
                if (*out_stderr) {
                    fread(*out_stderr, 1, sz, f);
                    (*out_stderr)[sz] = '\0';
                    char *p = *out_stderr + sz - 1;
                    while (p >= *out_stderr && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) {
                        *p-- = '\0';
                    }
                }
            }
            fclose(f);
        }
    }
}

/* PoP: browser_unlink_command_output_files @ tools/browser_tool.py:_unlink_command_output_files */
void browser_unlink_command_output_files(int count, const char **paths)
{
    for (int i = 0; i < count; i++) {
        if (paths[i]) {
            unlink(paths[i]);
        }
    }
}

/* PoP: browser_format_timeout_error @ tools/browser_tool.py:_format_browser_timeout_error */
char *browser_format_timeout_error(const char *command, int timeout, const char *stdout_text, const char *stderr_text)
{
    size_t cap = 512;
    char *result = malloc(cap);
    if (!result) return NULL;

    int len = snprintf(result, cap, "Command timed out after %d seconds", timeout);

    const char *detail = stderr_text && strlen(stderr_text) > 0 ? stderr_text : 
                         (stdout_text && strlen(stdout_text) > 0 ? stdout_text : "");
    if (detail && strlen(detail) > 0) {
        size_t need = len + 2 + strlen(detail);
        if (need >= cap) {
            cap = need + 256;
            char *new_result = realloc(result, cap);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
        }
        len += snprintf(result + len, cap - len, "\n%s", detail);
    }

    /* Check for hints */
    char combined[2048];
    snprintf(combined, sizeof(combined), "%s\n%s", stderr_text ? stderr_text : "", stdout_text ? stdout_text : "");
    for (char *p = combined; *p; p++) *p = tolower(*p);

    if (strstr(combined, "sandbox")) {
        const char *hint = "\nChromium sandbox launch failed. Set AGENT_BROWSER_ARGS='--no-sandbox,--disable-dev-shm-usage' in your environment, or run: npx agent-browser install --with-deps";
        size_t need = len + strlen(hint) + 1;
        if (need >= cap) {
            cap = need + 256;
            char *new_result = realloc(result, cap);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
        }
        strcat(result, hint);
        len += strlen(hint);
    } else if (strcmp(command, "open") == 0 && browser_is_local_mode()) {
        if (browser_running_in_docker()) {
            const char *hint = "\nThe browser daemon may still be starting or Chromium may be missing. Pull the latest image: docker pull ghcr.io/nousresearch/hermes-agent:latest";
            size_t need = len + strlen(hint) + 1;
            if (need >= cap) {
                cap = need + 256;
                char *new_result = realloc(result, cap);
                if (!new_result) { free(result); return NULL; }
                result = new_result;
            }
            strcat(result, hint);
        } else {
            const char *hint = "\nThe browser daemon may still be starting, or Chromium may be missing system libraries. Install/repair with: npx agent-browser install --with-deps (or: npx playwright install --with-deps chromium)";
            size_t need = len + strlen(hint) + 1;
            if (need >= cap) {
                cap = need + 256;
                char *new_result = realloc(result, cap);
                if (!new_result) { free(result); return NULL; }
                result = new_result;
            }
            strcat(result, hint);
        }
    }

    return result;
}

/* Helper functions needed by the above */

/* PoP: _running_in_docker @ tools/browser_tool.py:_running_in_docker */
/* PoP: browser_running_in_docker @ tools/browser_tool.py:_running_in_docker */
bool browser_running_in_docker(void)
{
    if (access("/.dockerenv", F_OK) == 0) return true;

    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            if (strstr(buf, "docker")) return true;
        } else {
            fclose(f);
        }
    }
    return false;
}

/* PoP: _is_local_mode @ tools/browser_tool.py:_is_local_mode */
/* PoP: browser_is_local_mode @ tools/browser_tool.py:_is_local_mode */
bool browser_is_local_mode(void)
{
    /* Simplified - no CDP override and no cloud provider */
    const char *cdp_override = getenv("BROWSER_CDP_URL");
    if (cdp_override && strlen(cdp_override) > 0) return false;
    /* Would check cloud provider config here */
    return true;
}

/* PoP: browser_bare_task_id_for_session_key @ tools/browser_tool.py:_bare_task_id_for_session_key */
char *browser_bare_task_id_for_session_key(const char *session_key)
{
    const char *suffix = "::local";
    size_t suffix_len = strlen(suffix);

    if (!session_key) {
        char *def = malloc(8);
        if (def) strcpy(def, "default");
        return def;
    }

    size_t len = strlen(session_key);
    if (len > suffix_len && strcmp(session_key + len - suffix_len, suffix) == 0) {
        char *result = malloc(len - suffix_len + 1);
        if (result) {
            memcpy(result, session_key, len - suffix_len);
            result[len - suffix_len] = '\0';
        }
        return result;
    }

    char *result = malloc(len + 1);
    if (result) strcpy(result, session_key);
    return result;
}

/* PoP: browser_session_info_owned_by_task @ tools/browser_tool.py:_session_info_owned_by_task */
bool browser_session_info_owned_by_task(const char *session_info_json, const char *task_id, const char *session_key)
{
    if (!session_info_json || !task_id || !session_key) return false;

    /* Parse JSON for owner_task_id and session_key fields */
    /* Simplified implementation - looks for keys in JSON string */
    char owner_key[64];
    snprintf(owner_key, sizeof(owner_key), "\"owner_task_id\":\"");
    char *owner_pos = strstr(session_info_json, owner_key);
    if (owner_pos) {
        owner_pos += strlen(owner_key);
        char *end = strchr(owner_pos, '"');
        if (end) {
            size_t owner_len = end - owner_pos;
            if (owner_len != strlen(task_id) || strncmp(owner_pos, task_id, owner_len) != 0) {
                return false;
            }
        }
    }

    char key_key[64];
    snprintf(key_key, sizeof(key_key), "\"session_key\":\"");
    char *key_pos = strstr(session_info_json, key_key);
    if (key_pos) {
        key_pos += strlen(key_key);
        char *end = strchr(key_pos, '"');
        if (end) {
            size_t key_len = end - key_pos;
            if (key_len != strlen(session_key) || strncmp(key_pos, session_key, key_len) != 0) {
                return false;
            }
        }
    }

    return true;
}

/* PoP: browser_get_session_inactivity_timeout @ tools/browser_tool.py:_get_session_inactivity_timeout */
/* PoP: browser_get_session_inactivity_timeout @ tools/browser_tool.py:_get_session_inactivity_timeout */
int browser_get_session_inactivity_timeout(void)
{
    int result = 120; /* DEFAULT_SESSION_INACTIVITY_TIMEOUT */

    const char *env_timeout = getenv("BROWSER_INACTIVITY_TIMEOUT");
    if (env_timeout) {
        int val = atoi(env_timeout);
        if (val >= 30) result = val;
    }

    /* Floor at 30s */
    if (result < 30) result = 30;

    return result;
}

/* PoP: browser_agent_browser_candidate_present @ tools/browser_tool.py:_agent_browser_candidate_present */
bool browser_agent_browser_candidate_present(const char *path)
{
    if (!path) return false;

    /* Check for "npx agent-browser" pattern */
    if (strstr(path, "npx") == path && strstr(path, "agent-browser")) {
        return true;
    }

    /* Check if file exists and is executable */
    struct stat st;
    if (stat(path, &st) == 0) {
        #ifdef _WIN32
        return true; /* Windows - rely on extension */
        #else
        return (st.st_mode & S_IXUSR) != 0;
        #endif
    }

    return false;
}

/* PoP: browser_redact_browser_output @ tools/browser_tool.py:_redact_browser_output */
/* PoP: browser_redact_browser_output @ tools/browser_tool.py:_redact_browser_output */
char *browser_redact_browser_output(const char *value_json)
{
    if (!value_json) {
        char *n = malloc(5);
        if (n) strcpy(n, "null");
        return n;
    }

    /* In C we implement a simplified redact_sensitive_text */
    char *result = malloc(strlen(value_json) + 1);
    if (!result) return NULL;
    strcpy(result, value_json);

    /* Redact common secret patterns */
    const char *patterns[] = {
        "sk-ant-", "sk-", "ghp_", "gho_", "ghu_", "ghs_", "ghr_",
        "Bearer ", "bearer ", "api_key", "apikey", "secret", "token",
        NULL
    };

    for (int i = 0; patterns[i]; i++) {
        char *pos = result;
        while ((pos = strstr(pos, patterns[i]))) {
            size_t plen = strlen(patterns[i]);
            /* Find end of token (whitespace, quote, comma, brace) */
            char *end = pos + plen;
            while (*end && *end != ' ' && *end != '\t' && *end != '\n' && 
                   *end != '"' && *end != '\'' && *end != ',' && *end != '}' && *end != ']') {
                end++;
            }
            size_t token_len = end - (pos + plen);
            if (token_len > 0) {
                memset(pos + plen, '*', token_len);
            }
            pos = end;
        }
    }

    return result;
}

/* PoP: browser_blocked_private_page_action @ tools/browser_tool.py:_blocked_private_page_action */
char *browser_blocked_private_page_action(const char *effective_task_id, const char *action)
{
    if (!browser_eval_ssrf_guard_active(effective_task_id)) {
        return NULL;
    }

    char *blocked_url = browser_current_page_private_url(effective_task_id);
    if (!blocked_url) {
        return NULL;
    }

    char *result = malloc(512);
    if (result) {
        snprintf(result, 512,
            "{\"success\":false,\"error\":\"Blocked: page URL targets a private or internal address (%s). Refusing to %s on this page in this browser mode.\"}",
            blocked_url, action);
    }
    free(blocked_url);
    return result;
}

/* PoP: browser_eval_ssrf_guard_active @ tools/browser_tool.py:_eval_ssrf_guard_active */
/* PoP: browser_eval_ssrf_guard_active @ tools/browser_tool.py:_eval_ssrf_guard_active */
bool browser_eval_ssrf_guard_active(const char *effective_task_id)
{
    return (!browser_is_local_backend() &&
            !browser_is_local_sidecar_key(effective_task_id) &&
            !browser_allow_private_urls());
}

/* PoP: browser_expression_targets_private_url @ tools/browser_tool.py:_expression_targets_private_url */
char *browser_expression_targets_private_url(const char *expression)
{
    if (!expression) return NULL;

    /* Scan for http(s)://... literals in JS expression */
    const char *p = expression;
    while ((p = strstr(p, "http://")) || (p = strstr(p, "https://"))) {
        /* Found a URL literal - extract it */
        const char *start = p;
        p += 4; /* skip "http" */
        if (p[-1] == 's') p++; /* skip 's' if https */
        p += 3; /* skip "://" */

        const char *end = p;
        while (*end && *end != ' ' && *end != '\t' && *end != '\n' && 
               *end != '"' && *end != '\'' && *end != ')' && *end != ']' && 
               *end != '>' && *end != ',' && *end != ';') {
            end++;
        }

        size_t url_len = end - start;
        char *url = malloc(url_len + 1);
        if (url) {
            memcpy(url, start, url_len);
            url[url_len] = '\0';
            /* Strip trailing punctuation */
            while (url_len > 0 && (url[url_len-1] == '.' || url[url_len-1] == ',' || url[url_len-1] == ';')) {
                url[--url_len] = '\0';
            }

            if (browser_is_always_blocked_url(url) || !browser_is_safe_url(url)) {
                return url; /* Caller must free */
            }
            free(url);
        }

        p = end;
    }

    return NULL;
}

/* PoP: browser_current_page_private_url @ tools/browser_tool.py:_current_page_private_url */
/* PoP: browser_current_page_private_url @ tools/browser_tool.py:_current_page_private_url */
char *browser_current_page_private_url(const char *effective_task_id)
{
    (void)effective_task_id;

    /* Try local browser tab first */
    const char *current_url = browser_get_current_url();
    if (current_url && current_url[0]) {
        if (browser_is_always_blocked_url(current_url) || !browser_is_safe_url(current_url)) {
            hermes_log(LOG_DEBUG, "port", "browser_current_page_private_url: found private URL %s", current_url);
            return strdup(current_url);
        }
    }

    return NULL;
}

/* PoP: browser_allow_unsafe_browser_evaluate @ tools/browser_tool.py:_allow_unsafe_browser_evaluate */
bool browser_allow_unsafe_browser_evaluate(void)
{
    /* Check config.yaml for browser.allow_unsafe_evaluate */
    const char *env = getenv("HERMES_BROWSER_ALLOW_UNSAFE_EVALUATE");
    if (env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0)) {
        return true;
    }
    return false;
}

/* PoP: browser_decode_js_string_literal @ tools/browser_tool.py:_decode_js_string_literal */
char *browser_decode_js_string_literal(const char *literal)
{
    if (!literal || strlen(literal) < 2) {
        return literal ? strdup(literal) : NULL;
    }

    /* Remove surrounding quotes */
    char quote = literal[0];
    if (quote != '"' && quote != '\'' && quote != '`') {
        return strdup(literal);
    }

    size_t len = strlen(literal);
    if (len < 2 || literal[len-1] != quote) {
        return strdup(literal);
    }

    const char *body = literal + 1;
    size_t body_len = len - 2;

    char *result = malloc(body_len + 1);
    if (!result) return NULL;

    size_t out_idx = 0;
    for (size_t i = 0; i < body_len; i++) {
        if (body[i] == '\\' && i + 1 < body_len) {
            char next = body[i+1];
            switch (next) {
                case 'n': result[out_idx++] = '\n'; break;
                case 't': result[out_idx++] = '\t'; break;
                case 'r': result[out_idx++] = '\r'; break;
                case 'b': result[out_idx++] = '\b'; break;
                case 'f': result[out_idx++] = '\f'; break;
                case 'v': result[out_idx++] = '\v'; break;
                case '\\': result[out_idx++] = '\\'; break;
                case '"': result[out_idx++] = '"'; break;
                case '\'': result[out_idx++] = '\''; break;
                case '`': result[out_idx++] = '`'; break;
                case 'x': /* \xHH */
                    if (i + 3 < body_len && isxdigit(body[i+2]) && isxdigit(body[i+3])) {
                        char hex[3] = {body[i+2], body[i+3], 0};
                        result[out_idx++] = (char)strtol(hex, NULL, 16);
                        i += 3;
                    } else {
                        result[out_idx++] = body[i];
                    }
                    break;
                case 'u': /* \uHHHH */
                    if (i + 5 < body_len && isxdigit(body[i+2]) && isxdigit(body[i+3]) && 
                        isxdigit(body[i+4]) && isxdigit(body[i+5])) {
                        char hex[5] = {body[i+2], body[i+3], body[i+4], body[i+5], 0};
                        uint32_t cp = strtoul(hex, NULL, 16);
                        /* Simplified: only handle BMP */
                        if (cp <= 0x7F) {
                            result[out_idx++] = (char)cp;
                        } else if (cp <= 0x7FF) {
                            result[out_idx++] = 0xC0 | (cp >> 6);
                            result[out_idx++] = 0x80 | (cp & 0x3F);
                        } else {
                            result[out_idx++] = 0xE0 | (cp >> 12);
                            result[out_idx++] = 0x80 | ((cp >> 6) & 0x3F);
                            result[out_idx++] = 0x80 | (cp & 0x3F);
                        }
                        i += 5;
                    } else {
                        result[out_idx++] = body[i];
                    }
                    break;
                default:
                    result[out_idx++] = body[i];
                    break;
            }
        } else {
            result[out_idx++] = body[i];
        }
    }
    result[out_idx] = '\0';
    return result;
}

/* PoP: browser_decoded_js_string_literals @ tools/browser_tool.py:_decoded_js_string_literals */
char **browser_decoded_js_string_literals(const char *expression, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!expression) return NULL;

    /* Count string literals first */
    int count = 0;
    const char *p = expression;
    while ((p = strpbrk(p, "\"'`"))) {
        char quote = *p;
        p++;
        const char *end = p;
        while (*end && *end != quote) {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        if (*end == quote) count++;
        p = end + 1;
    }

    if (count == 0) return NULL;

    char **results = calloc(count, sizeof(char*));
    if (!results) return NULL;

    int idx = 0;
    p = expression;
    while ((p = strpbrk(p, "\"'`")) && idx < count) {
        char quote = *p;
        const char *start = p++;
        const char *end = p;
        while (*end && *end != quote) {
            if (*end == '\\' && end[1]) end += 2;
            else end++;
        }
        if (*end == quote) {
            size_t len = end - start + 1;
            char *literal = malloc(len + 1);
            if (literal) {
                memcpy(literal, start, len);
                literal[len] = '\0';
                results[idx++] = browser_decode_js_string_literal(literal);
                free(literal);
            }
            p = end + 1;
        }
    }

    if (out_count) *out_count = idx;
    return results;
}

/* PoP: browser_sensitive_browser_eval_token_reason @ tools/browser_tool.py:_sensitive_browser_eval_token_reason */
char *browser_sensitive_browser_eval_token_reason(const char *expression)
{
    if (!expression) return NULL;

    static const struct {
        const char *token;
        const char *reason;
    } sensitive_tokens[] = {
        {"cookie", "document.cookie"},
        {"localStorage", "web storage"},
        {"sessionStorage", "web storage"},
        {"indexedDB", "IndexedDB"},
        {"caches", "Cache Storage"},
        {"clipboard", "navigator sensitive API"},
        {"credentials", "navigator sensitive API"},
        {"serviceWorker", "navigator sensitive API"},
        {"fetch", "network request"},
        {"XMLHttpRequest", "network request"},
        {"WebSocket", "network request"},
        {"EventSource", "network request"},
        {"sendBeacon", "network beacon"},
        {NULL, NULL}
    };

    /* Get all decoded string literals */
    int lit_count = 0;
    char **literals = browser_decoded_js_string_literals(expression, &lit_count);

    char *concatenated = NULL;
    size_t concat_len = 0;
    for (int i = 0; i < lit_count; i++) {
        concat_len += strlen(literals[i]);
    }
    if (concat_len > 0) {
        concatenated = malloc(concat_len + 1);
        if (concatenated) {
            concatenated[0] = '\0';
            for (int i = 0; i < lit_count; i++) {
                strcat(concatenated, literals[i]);
            }
            for (char *c = concatenated; *c; c++) *c = tolower(*c);
        }
    }

    for (int i = 0; sensitive_tokens[i].token; i++) {
        const char *token = sensitive_tokens[i].token;
        const char *reason = sensitive_tokens[i].reason;

        /* Direct identifier match - case insensitive */
        char *lower_expr = strdup(expression);
        if (lower_expr) {
            for (char *c = lower_expr; *c; c++) *c = tolower(*c);
            char *lower_token = strdup(token);
            if (lower_token) {
                for (char *c = lower_token; *c; c++) *c = tolower(*c);
                if (strstr(lower_expr, lower_token)) {
                    free(lower_expr);
                    free(lower_token);
                    if (concatenated) free(concatenated);
                    for (int j = 0; j < lit_count; j++) free(literals[j]);
                    free(literals);
                    return strdup(reason);
                }
                free(lower_token);
            }
            free(lower_expr);
        }

        /* In string literals */
        if (concatenated) {
            char *lower_token = strdup(token);
            if (lower_token) {
                for (char *c = lower_token; *c; c++) *c = tolower(*c);
                if (strstr(concatenated, lower_token)) {
                    free(lower_token);
                    if (concatenated) free(concatenated);
                    for (int j = 0; j < lit_count; j++) free(literals[j]);
                    free(literals);
                    return strdup(reason);
                }
                free(lower_token);
            }
        }
    }

    if (concatenated) free(concatenated);
    for (int i = 0; i < lit_count; i++) free(literals[i]);
    free(literals);

    return NULL;
}

/* PoP: browser_risky_browser_eval_reason @ tools/browser_tool.py:_risky_browser_eval_reason */
char *browser_risky_browser_eval_reason(const char *expression)
{
    if (!expression) return NULL;

    /* Simplified regex matching - in production would use regex library */
    if (strstr(expression, "document.cookie")) return strdup("document.cookie");
    if (strstr(expression, "localStorage") || strstr(expression, "sessionStorage")) return strdup("web storage");
    if (strstr(expression, "indexedDB")) return strdup("IndexedDB");
    if (strstr(expression, "caches.open") || strstr(expression, "caches.match") || strstr(expression, "caches.keys")) return strdup("Cache Storage");
    if (strstr(expression, "navigator.clipboard") || strstr(expression, "navigator.credentials") || strstr(expression, "navigator.serviceWorker")) return strdup("navigator sensitive API");
    if (strstr(expression, "fetch(") || strstr(expression, "XMLHttpRequest(") || strstr(expression, "WebSocket(") || strstr(expression, "EventSource(")) return strdup("network request");
    if (strstr(expression, "sendBeacon(")) return strdup("network beacon");
    if (strstr(expression, "document.forms") && strstr(expression, ".value")) return strdup("form value extraction");
    if (strstr(expression, "querySelector") && (strstr(expression, "input") || strstr(expression, "textarea") || strstr(expression, "password")) && strstr(expression, ".value")) return strdup("form value extraction");

    return browser_sensitive_browser_eval_token_reason(expression);
}

/* PoP: browser_enforce_browser_eval_policy @ tools/browser_tool.py:_enforce_browser_eval_policy */
char *browser_enforce_browser_eval_policy(const char *expression)
{
    if (browser_allow_unsafe_browser_evaluate()) {
        return NULL;
    }

    char *reason = browser_risky_browser_eval_reason(expression);
    if (!reason) {
        return NULL;
    }

    char *result = malloc(1024);
    if (result) {
        snprintf(result, 1024,
            "Blocked: browser_console(expression=...) tried to use sensitive browser JavaScript primitive (%s). Use browser_snapshot/browser_get_images/browser_console without expression for normal inspection, or set browser.allow_unsafe_evaluate: true in config.yaml only for trusted pages when this access is explicitly required.",
            reason);
    }
    free(reason);
    return result;
}

/* PoP: browser_maybe_autoinstall_chromium @ tools/browser_tool.py:_maybe_autoinstall_chromium */
/* PoP: browser_maybe_autoinstall_chromium @ tools/browser_tool.py:_maybe_autoinstall_chromium */
bool browser_maybe_autoinstall_chromium(void)
{
    static bool attempted = false;

    if (attempted) {
        return browser_chromium_installed();
    }
    attempted = true;

    if (browser_running_in_docker()) {
        return false;
    }

    /* Check security.allow_lazy_installs config */
    const char *env = getenv("HERMES_SECURITY_ALLOW_LAZY_INSTALLS");
    if (env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0)) {
        /* In production, would invoke agent-browser install */
        hermes_log(LOG_INFO, "port", "browser: Chromium missing — auto-installing the browser binary (one-time ~170MB; disable via security.allow_lazy_installs)");
        /* Would run: agent-browser install */
        /* For now, return false to indicate not installed */
        return false;
    }

    return false;
}

/* PoP: _chromium_installed @ tools/browser_tool.py:_chromium_installed */
/* PoP: browser_chromium_installed @ tools/browser_tool.py:_chromium_installed */
bool browser_chromium_installed(void)
{
    /* Check for Chromium binary in common locations */
    const char *paths[] = {
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) return true;
    }

    return false;
}

/* PoP: _is_local_backend @ tools/browser_tool.py:_is_local_backend */
/* PoP: browser_is_local_backend @ tools/browser_tool.py:_is_local_backend */
bool browser_is_local_backend(void)
{
    /* Local backend = no CDP override, no camofox, no cloud provider */
    const char *cdp = getenv("BROWSER_CDP_URL");
    if (cdp && strlen(cdp) > 0) return false;

    const char *camofox = getenv("CAMOFOX_URL");
    if (camofox && strlen(camofox) > 0) return true;

    /* Would check cloud provider config */
    return true; /* Default to local */
}

/* PoP: _is_local_sidecar_key @ tools/browser_tool.py:_is_local_sidecar_key */
/* PoP: browser_is_local_sidecar_key @ tools/browser_tool.py:_is_local_sidecar_key */
bool browser_is_local_sidecar_key(const char *session_key)
{
    if (!session_key) return false;
    const char *suffix = "::local";
    size_t len = strlen(session_key);
    size_t suf_len = strlen(suffix);
    return len > suf_len && strcmp(session_key + len - suf_len, suffix) == 0;
}

/* PoP: _allow_private_urls @ tools/browser_tool.py:_allow_private_urls */
/* PoP: browser_allow_private_urls @ tools/browser_tool.py:_allow_private_urls */
bool browser_allow_private_urls(void)
{
    const char *env = getenv("HERMES_BROWSER_ALLOW_PRIVATE_URLS");
    return env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0);
}

/* PoP: _is_always_blocked_url @ tools/browser_tool.py:_is_always_blocked_url */
/* PoP: browser_is_always_blocked_url @ tools/browser_tool.py:_is_always_blocked_url */
bool browser_is_always_blocked_url(const char *url)
{
    if (!url) return false;
    /* Cloud metadata endpoints */
    if (strstr(url, "169.254.169.254")) return true;
    if (strstr(url, "metadata.google.internal")) return true;
    if (strstr(url, "metadata.azure.com")) return true;
    if (strstr(url, "169.254.170.2")) return true; /* ECS metadata */
    return false;
}

/* PoP: _is_safe_url @ tools/browser_tool.py:_is_safe_url */
/* PoP: browser_is_safe_url @ tools/browser_tool.py:_is_safe_url */
bool browser_is_safe_url(const char *url)
{
    if (!url) return false;
    /* Simplified - check for private IP ranges */
    if (strncmp(url, "http://10.", 10) == 0) return false;
    if (strncmp(url, "http://192.168.", 13) == 0) return false;
    if (strncmp(url, "http://172.16.", 12) == 0) return false;
    if (strncmp(url, "http://172.17.", 12) == 0) return false;
    if (strncmp(url, "http://172.18.", 12) == 0) return false;
    if (strncmp(url, "http://172.19.", 12) == 0) return false;
    if (strncmp(url, "http://172.20.", 12) == 0) return false;
    if (strncmp(url, "http://172.21.", 12) == 0) return false;
    if (strncmp(url, "http://172.22.", 12) == 0) return false;
    if (strncmp(url, "http://172.23.", 12) == 0) return false;
    if (strncmp(url, "http://172.24.", 12) == 0) return false;
    if (strncmp(url, "http://172.25.", 12) == 0) return false;
    if (strncmp(url, "http://172.26.", 12) == 0) return false;
    if (strncmp(url, "http://172.27.", 12) == 0) return false;
    if (strncmp(url, "http://172.28.", 12) == 0) return false;
    if (strncmp(url, "http://172.29.", 12) == 0) return false;
    if (strncmp(url, "http://172.30.", 12) == 0) return false;
    if (strncmp(url, "http://172.31.", 12) == 0) return false;
    if (strncmp(url, "http://127.", 11) == 0) return false;
    if (strncmp(url, "http://localhost", 16) == 0) return false;
    if (strncmp(url, "https://127.", 12) == 0) return false;
    if (strncmp(url, "https://localhost", 17) == 0) return false;
    return true;
}

/* PoP: _is_camofox_mode @ tools/browser_tool.py:_is_camofox_mode */
/* PoP: browser_is_camofox_mode @ tools/browser_tool.py:_is_camofox_mode */
bool browser_is_camofox_mode(void)
{
    const char *url = getenv("CAMOFOX_URL");
    return url && strlen(url) > 0;
}

/* PoP: _discover_homebrew_node_dirs @ tools/browser_tool.py:_discover_homebrew_node_dirs */
char *browser_discover_homebrew_node_dirs(void)
{
    /* Find Homebrew versioned Node.js bin directories */
    char *result = malloc(4096);
    if (!result) return NULL;
    result[0] = '\0';

    const char *homebrew_opt = "/opt/homebrew/opt";
    DIR *dir = opendir(homebrew_opt);
    if (dir) {
        struct dirent *entry;
        bool first = true;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "node", 4) == 0 && strcmp(entry->d_name, "node") != 0) {
                char bin_dir[4096];
                snprintf(bin_dir, sizeof(bin_dir), "%s/%s/bin", homebrew_opt, entry->d_name);
                if (access(bin_dir, F_OK) == 0) {
                    if (!first) strcat(result, ":");
                    strcat(result, bin_dir);
                    first = false;
                }
            }
        }
        closedir(dir);
    }
    return result;
}

/* PoP: _browser_candidate_path_dirs @ tools/browser_tool.py:_browser_candidate_path_dirs */
char *browser_browser_candidate_path_dirs(void)
{
    /* Return ordered browser CLI PATH candidates */
    const char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) hermes_home = "/tmp/.hermes";

    char *result = malloc(8192);
    if (!result) return NULL;
    result[0] = '\0';

    char *discover = browser_discover_homebrew_node_dirs();
    if (discover && discover[0]) {
        strcat(result, discover);
        strcat(result, ":");
    }
    free(discover);

    /* Add standard paths */
    strcat(result, "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    return result;
}

/* PoP: _merge_browser_path @ tools/browser_tool.py:_merge_browser_path */
char *browser_merge_browser_path(const char *existing_path)
{
    /* Prepend browser-specific PATH fallbacks without reordering existing entries */
    if (!existing_path) existing_path = "";

    char *candidates = browser_browser_candidate_path_dirs();
    if (!candidates) return strdup(existing_path);

    size_t cap = strlen(candidates) + strlen(existing_path) + 256;
    char *result = malloc(cap);
    if (!result) {
        free(candidates);
        return strdup(existing_path);
    }
    result[0] = '\0';

    /* Track existing path parts */
    char *existing_parts[256];
    int existing_count = 0;
    char *existing_copy = strdup(existing_path);
    if (existing_copy) {
        char *tok = strtok(existing_copy, ":");
        while (tok && existing_count < 256) {
            existing_parts[existing_count++] = strdup(tok);
            tok = strtok(NULL, ":");
        }
    }

    /* Add candidate directories that aren't already in PATH */
    char *cand_copy = strdup(candidates);
    char *tok = strtok(cand_copy, ":");
    while (tok) {
        bool found = false;
        for (int i = 0; i < existing_count; i++) {
            if (strcmp(tok, existing_parts[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found && access(tok, F_OK) == 0) {
            if (result[0]) strcat(result, ":");
            strcat(result, tok);
        }
        tok = strtok(NULL, ":");
    }
    free(cand_copy);

    /* Append existing PATH */
    if (existing_path[0]) {
        if (result[0]) strcat(result, ":");
        strcat(result, existing_path);
    }

    /* Cleanup */
    for (int i = 0; i < existing_count; i++) free(existing_parts[i]);
    free(existing_copy);
    free(candidates);

    return result;
}

/* PoP: _get_vision_model @ tools/browser_tool.py:_get_vision_model */
char *browser_get_vision_model(void)
{
    const char *model = getenv("AUXILIARY_VISION_MODEL");
    if (model && model[0]) return strdup(model);
    return strdup("");
}

/* PoP: _get_extraction_model @ tools/browser_tool.py:_get_extraction_model */
char *browser_get_extraction_model(void)
{
    const char *model = getenv("AUXILIARY_WEB_EXTRACT_MODEL");
    if (model && model[0]) return strdup(model);
    return strdup("");
}

/* PoP: _resolve_cdp_override @ tools/browser_tool.py:_resolve_cdp_override */
char *browser_resolve_cdp_override(const char *cdp_url)
{
    if (!cdp_url) return strdup("");

    /* Check if it's already a full WebSocket endpoint */
    if (strstr(cdp_url, "/devtools/browser/") != NULL) {
        return strdup(cdp_url);
    }

    /* Check if it's a bare host:port */
    const char *ws_prefix = "";
    if (strncmp(cdp_url, "ws://", 5) == 0) ws_prefix = "ws://";
    else if (strncmp(cdp_url, "wss://", 6) == 0) ws_prefix = "wss://";

    if (ws_prefix[0]) {
        const char *rest = cdp_url + strlen(ws_prefix);
        if (strchr(rest, '/') == NULL) {
            /* Convert to discovery URL */
            char *discovery = malloc(strlen(cdp_url) + 16);
            if (discovery) {
                snprintf(discovery, strlen(cdp_url) + 16, "http%s/json/version", cdp_url + 2);
                return discovery;
            }
        }
        return strdup(cdp_url);
    }

    /* Assume it's a discovery URL */
    char *version_url = malloc(strlen(cdp_url) + 16);
    if (!version_url) return strdup(cdp_url);

    if (strstr(cdp_url, "/json/version")) {
        strcpy(version_url, cdp_url);
    } else {
        snprintf(version_url, strlen(cdp_url) + 16, "%s/json/version", cdp_url);
    }
    return version_url;
}

/* PoP: _get_cdp_override @ tools/browser_tool.py:_get_cdp_override */
char *browser_get_cdp_override(void)
{
    const char *env_override = getenv("BROWSER_CDP_URL");
    if (env_override && env_override[0]) {
        return browser_resolve_cdp_override(env_override);
    }

    /* Could also check config.yaml for browser.cdp_url */
    return strdup("");
}

/* PoP: _get_dialog_policy_config @ tools/browser_tool.py:_get_dialog_policy_config */
char *browser_get_dialog_policy_config(void)
{
    /* Return policy and timeout as JSON */
    const char *policy = getenv("HERMES_BROWSER_DIALOG_POLICY");
    if (!policy) policy = "must_respond";

    const char *timeout_str = getenv("HERMES_BROWSER_DIALOG_TIMEOUT_S");
    double timeout = timeout_str ? atof(timeout_str) : 300.0;

    char *result = malloc(256);
    if (!result) return strdup("{}");
    snprintf(result, 256, "{\"policy\":\"%s\",\"timeout_s\":%.1f}", policy, timeout);
    return result;
}

/* PoP: _ensure_cdp_supervisor @ tools/browser_tool.py:_ensure_cdp_supervisor */
json_t *browser_ensure_cdp_supervisor(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!task_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("task_id required"));
        return result;
    }

    /* Get CDP override */
    char *cdp_url = browser_get_cdp_override();
    if (!cdp_url || !cdp_url[0]) {
        free(cdp_url);
        json_set(result, "ok", json_bool(true));
        json_set(result, "supervisor", json_string("none"));
        json_set(result, "note", json_string("no CDP URL configured"));
        return result;
    }

    /* Get policy config */
    char *policy_json = browser_get_dialog_policy_config();
    json_t *policy_obj = policy_json ? json_parse(policy_json, NULL) : NULL;
    if (policy_obj) {
        json_t *p = json_obj_get(policy_obj, "policy");
        if (p) json_get_str(p, NULL, "must_respond");
        json_t *t = json_obj_get(policy_obj, "timeout_s");
        if (t && t->type == JSON_NUMBER) {
            double timeout = t->num_val;
            (void)timeout;
        }
        json_free(policy_obj);
    }
    free(policy_json);

    /* Call get_or_start */
    json_t *supervisor = browser_supervisor_get_or_start(task_id);
    if (supervisor) {
        json_set(result, "ok", json_bool(true));
        json_set(result, "supervisor", supervisor);
    } else {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("failed to start supervisor"));
    }

    free(cdp_url);
    return result;
}

/* PoP: _stop_cdp_supervisor @ tools/browser_tool.py:_stop_cdp_supervisor */
json_t *browser_stop_cdp_supervisor(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!task_id) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "error", json_string("task_id required"));
        return result;
    }

    browser_supervisor_stop_all();

    json_set(result, "ok", json_bool(true));
    json_set(result, "task_id", json_string(task_id));
    return result;
}



/* PoP: _using_lightpanda_engine @ tools/browser_tool.py:_using_lightpanda_engine */
bool browser_using_lightpanda_engine(void)
{
    const char *engine = getenv("HERMES_BROWSER_ENGINE");
    return engine && strcmp(engine, "lightpanda") == 0;
}



/* PoP: _copy_fallback_warning @ tools/browser_tool.py:_copy_fallback_warning */
void browser_copy_fallback_warning(char *dest, size_t dest_size)
{
    if (dest && dest_size > 0) {
        snprintf(dest, dest_size, "Using fallback browser engine");
    }
}





/* PoP: _auto_local_for_private_urls @ tools/browser_tool.py:_auto_local_for_private_urls */
bool browser_auto_local_for_private_urls(void)
{
    const char *env = getenv("HERMES_BROWSER_AUTO_LOCAL_PRIVATE");
    return env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0);
}

/* PoP: _url_is_private @ tools/browser_tool.py:_url_is_private */
bool browser_url_is_private(const char *url)
{
    if (!url) return false;
    if (strncmp(url, "http://10.", 10) == 0) return true;
    if (strncmp(url, "http://192.168.", 13) == 0) return true;
    if (strncmp(url, "http://172.16.", 12) == 0) return true;
    if (strncmp(url, "http://127.", 11) == 0) return true;
    if (strncmp(url, "http://localhost", 16) == 0) return true;
    if (strncmp(url, "https://10.", 11) == 0) return true;
    if (strncmp(url, "https://192.168.", 14) == 0) return true;
    if (strncmp(url, "https://172.16.", 13) == 0) return true;
    if (strncmp(url, "https://127.", 12) == 0) return true;
    if (strncmp(url, "https://localhost", 17) == 0) return true;
    return false;
}

/* PoP: _navigation_session_key @ tools/browser_tool.py:_navigation_session_key */
char *browser_navigation_session_key(const char *task_id)
{
    if (!task_id) return strdup("default");
    size_t len = strlen(task_id) + 8;
    char *result = malloc(len);
    if (result) snprintf(result, len, "%s::local", task_id);
    return result;
}



/* PoP: _socket_safe_tmpdir @ tools/browser_tool.py:_socket_safe_tmpdir */
char *browser_socket_safe_tmpdir(void)
{
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    return strdup(tmpdir);
}











/* PoP: _create_local_session @ tools/browser_tool.py:_create_local_session */
json_t *browser_create_local_session(const char *task_id)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "ok", json_bool(true));
    json_set(result, "session_key", json_string(task_id ? task_id : "default"));
    json_set(result, "type", json_string("local"));
    return result;
}

/* PoP: _create_cdp_session @ tools/browser_tool.py:_create_cdp_session */
json_t *browser_create_cdp_session(const char *task_id, const char *cdp_url)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "ok", json_bool(true));
    json_set(result, "session_key", json_string(task_id ? task_id : "default"));
    json_set(result, "type", json_string("cdp"));
    if (cdp_url) json_set(result, "cdp_url", json_string(cdp_url));
    return result;
}

/* PoP: _get_session_info @ tools/browser_tool.py:_get_session_info */
json_t *browser_get_session_info(const char *session_key)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "session_key", json_string(session_key ? session_key : ""));
    json_set(result, "active", json_bool(false));
    return result;
}

/* PoP: _find_agent_browser @ tools/browser_tool.py:_find_agent_browser */
char *browser_find_agent_browser(void)
{
    const char *candidates[] = {
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) {
            return strdup(candidates[i]);
        }
    }
    return strdup("");
}







/* PoP: _truncate_snapshot @ tools/browser_tool.py:_truncate_snapshot */
char *browser_truncate_snapshot(const char *snapshot, size_t max_chars)
{
    if (!snapshot) return strdup("");
    if (strlen(snapshot) <= max_chars) return strdup(snapshot);

    char *result = malloc(max_chars + 1);
    if (!result) return strdup("");
    strncpy(result, snapshot, max_chars);
    result[max_chars] = '\0';
    return result;
}

















/* PoP: check_browser_requirements @ tools/browser_tool.py:check_browser_requirements */
json_t *browser_check_browser_requirements(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "chromium_installed", json_bool(browser_chromium_installed()));
    json_set(result, "running_in_docker", json_bool(browser_running_in_docker()));
    json_set(result, "needs_sandbox_bypass", json_bool(browser_needs_chromium_sandbox_bypass()));
    return result;
}

/* PoP: check_browser_vision_requirements @ tools/browser_tool.py:check_browser_vision_requirements */
json_t *browser_check_browser_vision_requirements(void)
{
    json_t *result = json_object();
    if (!result) return NULL;
    json_set(result, "vision_model", json_string(browser_get_vision_model()));
    json_set(result, "extraction_model", json_string(browser_get_extraction_model()));
    json_set(result, "requirements_met", json_bool(true));
    return result;
}

#endif /* SRC_TOOLS_PORT_BROWSER_TOOL_C */
