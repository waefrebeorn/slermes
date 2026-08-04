/*
 * browser_tool_env.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_env.h"
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
#include <sys/stat.h>

#include "browser_tool_platform.h"

struct browser_tool_env {
    int unused;
};

browser_tool_env_t *browser_tool_env_init(void) { return calloc(1, sizeof(browser_tool_env_t)); }
void browser_tool_env_cleanup(browser_tool_env_t *s) { free(s); }

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
            const char *hint = "\nThe browser daemon may still be starting or Chromium may be missing. Pull the latest image: docker pull ghcr.io/waefrebeorn/slermes:latest";
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

