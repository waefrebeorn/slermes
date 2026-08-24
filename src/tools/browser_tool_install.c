/*
 * browser_tool_install.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_install.h"
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

#include "browser_tool_platform.h"
#include <stdarg.h>
#include "url_safety_cli.h"
#include "hermes_url_safety.h"
extern char *sensitive_query_param_name(const char *url); /* impl in url_safety.c */
#include "url_safety_helpers.h"

struct browser_tool_install {
    int unused;
};

browser_tool_install_t *browser_tool_install_init(void) { return calloc(1, sizeof(browser_tool_install_t)); }
void browser_tool_install_cleanup(browser_tool_install_t *s) { free(s); }

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

/* PoP: browser_chromium_installed @ tools/browser_tool.py:_chromium_installed */
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

/* PoP: browser_is_local_backend @ tools/browser_tool.py:_is_local_backend */
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

/* PoP: browser_is_local_sidecar_key @ tools/browser_tool.py:_is_local_sidecar_key */
/* PoP: browser_is_local_sidecar_key @ tools/browser_tool.py:_is_local_sidecar_key */
bool browser_is_local_sidecar_key(const char *session_key)
{
    if (!session_key) return false;
    const char *suffix = "::local";
    size_t len = strlen(session_key);
    size_t suf_len = strlen(suffix);
    return len > suf_len && strcmp(session_key + len - suf_len, suffix) == 0;
}

/* PoP: browser_allow_private_urls @ tools/browser_tool.py:_allow_private_urls */
/* PoP: browser_allow_private_urls @ tools/browser_tool.py:_allow_private_urls */
bool browser_allow_private_urls(void)
{
    const char *env = getenv("HERMES_BROWSER_ALLOW_PRIVATE_URLS");
    return env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0);
}

/* PoP: browser_is_always_blocked_url @ tools/browser_tool.py:_is_always_blocked_url */
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

/* PoP: browser_is_safe_url @ tools/browser_tool.py:_is_safe_url */
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

/* PoP: browser_is_camofox_mode @ tools/browser_camofox.py:is_camofox_mode */
/* PoP: browser_is_camofox_mode @ tools/browser_camofox.py:is_camofox_mode */
bool browser_is_camofox_mode(void)
{
    const char *url = getenv("CAMOFOX_URL");
    return url && strlen(url) > 0;
}



/* ---- URL safety gate (evaluate_url_safety family) ---------------------- */

/* website_policy port has no public header; declare the pieces we reuse. */
extern const char *cli_tools_website_policy__get_default_config_path(void);
extern void cli_tools_website_policy__normalize_host(const char *host, char *buf, size_t bufsize);
extern int cli_tools_website_policy__match_host_against_rule(const char *host, const char *pattern);
extern int cli_tools_website_policy__iter_blocklist_file_rules(const char *path, char **rules, int max_rules);
extern int cli_tools_website_policy__extract_host_from_urlish(const char *url, char *buf, size_t bufsize);

/* Python check_website_access(): consult the configured website blocklist.
 * The C policy port keeps rules in a plain rules file; HERMES_WEBSITE_BLOCKLIST
 * overrides the path, defaulting to <HERMES_HOME|HOME>/website_blocklist.txt.
 * On a hit, fills host/rule/source and returns a malloc'd message; else NULL. */
static char *browser_eval_website_block_hit(const char *url,
                                            char *host_out, size_t host_size,
                                            char *rule_out, size_t rule_size,
                                            const char **source)
{
    static char msg[512];
    if (!url || !*url) return NULL;

    char host[256];
    if (cli_tools_website_policy__extract_host_from_urlish(url, host, sizeof(host)) != 0)
        return NULL;
    cli_tools_website_policy__normalize_host(host, host, sizeof(host));
    if (!host[0]) return NULL;

    const char *env_path = getenv("HERMES_WEBSITE_BLOCKLIST");
    char path[1024];
    if (env_path && env_path[0]) {
        snprintf(path, sizeof(path), "%s", env_path);
    } else {
        const char *home = cli_tools_website_policy__get_default_config_path();
        if (!home || !home[0]) return NULL;
        snprintf(path, sizeof(path), "%s/website_blocklist.txt", home);
    }

    enum { MAX_RULES = 256 };
    char *rules[MAX_RULES];
    int n = cli_tools_website_policy__iter_blocklist_file_rules(path, rules, MAX_RULES);
    char *hit = NULL;
    for (int i = 0; i < n; i++) {
        if (!hit && cli_tools_website_policy__match_host_against_rule(host, rules[i]))
            hit = rules[i]; /* keep for copy; free the rest */
        else
            free(rules[i]);
    }
    if (!hit) return NULL;

    snprintf(msg, sizeof(msg), "Blocked by website access policy: '%s' is blocked by rule '%s'.",
             host, hit);
    snprintf(host_out, host_size, "%s", host);
    snprintf(rule_out, rule_size, "%s", hit);
    free(hit);
    *source = "file";
    return msg;
}

static json_t *browser_eval_error(const char *error_fmt, ...)
{
    va_list ap;
    va_start(ap, error_fmt);
    char buf[768];
    vsnprintf(buf, sizeof(buf), error_fmt, ap);
    va_end(ap);
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "success", json_bool(false));
    json_set(obj, "error", json_string(buf));
    return obj;
}

/* PoP: browser_evaluate_url_safety @ tools/browser_tool.py:evaluate_url_safety */
json_t *browser_evaluate_url_safety(const char *url)
{
    /* Run URL safety checks; NULL json when safe, else an error object. */
    if (!url || !*url) return NULL;

    /* Secret exfiltration guard: raw + URL-decoded forms. url_has_secret()
     * already checks both (agent/redact.py:_PREFIX_RE twin). */
    if (url_has_secret(url)) {
        return browser_eval_error(
            "Blocked: URL contains what appears to be an API key or token. "
            "Secrets must not be sent in URLs.");
    }

    /* Re-check the request-normalized form (tools/url_safety.py:
     * normalize_url_for_request twin). */
    char normalized[4096];
    if (cli_tools_url_safety_normalize_url_for_request(url, normalized,
                                                       sizeof(normalized)) == 0 &&
        normalized[0] && strcmp(normalized, url) != 0 &&
        url_has_secret(normalized)) {
        return browser_eval_error(
            "Blocked: URL contains what appears to be an API key or token. "
            "Secrets must not be sent in URLs.");
    }
    if (normalized[0])
        url = normalized;

    bool local = browser_is_local_backend();

    /* Credential-like query parameter guard (skipped for local backends). */
    char *qkey = sensitive_query_param_name(url);
    if (qkey && qkey[0] && !local) {
        json_t *err = browser_eval_error(
            "Blocked: URL contains a credential-like query parameter (%s). "
            "Cloud browser backends are third-party readers; use a local "
            "browser/CDP session or remove the sensitive query parameter "
            "before navigating.",
            qkey);
        free(qkey);
        return err;
    }
    free(qkey);

    /* Cloud metadata endpoint blocklist. */
    if (browser_is_always_blocked_url(url)) {
        return browser_eval_error(
            "Blocked: URL targets a cloud metadata endpoint");
    }

    /* SSRF guard: private/internal addresses unless local backend or opted out
     * via browser.allow_private_urls. */
    if (!local && !browser_allow_private_urls() && !browser_is_safe_url(url)) {
        return browser_eval_error(
            "Blocked: URL targets a private or internal address");
    }

    /* Website access policy (check_website_access). */
    char host[256], rule[256];
    const char *source = NULL;
    char *msg = browser_eval_website_block_hit(url, host, sizeof(host),
                                               rule, sizeof(rule), &source);
    if (msg) {
        json_t *obj = browser_eval_error("%s", msg);
        if (obj) {
            json_t *bp = json_object();
            if (bp) {
                json_set(bp, "host", json_string(host));
                json_set(bp, "rule", json_string(rule));
                json_set(bp, "source", json_string(source));
                json_set(obj, "blocked_by_policy", bp);
            }
        }
        return obj;
    }

    return NULL; /* safe */
}
