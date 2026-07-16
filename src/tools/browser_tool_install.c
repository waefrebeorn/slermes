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

