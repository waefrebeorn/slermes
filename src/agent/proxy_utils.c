/**
 * @file proxy_utils.c
 * @brief HTTP proxy resolution from environment variables.
 *
 * Port of Python process_bootstrap._get_proxy_from_env() and
 * _get_proxy_for_base_url() from agent/process_bootstrap.py.
 *
 * Also ports normalize_proxy_url() from utils.py.
 */

#include <stddef.h>
#include "hermes_proxy_utils.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- helpers ---------- */

/**
 * Normalize proxy URL for HTTP client compatibility.
 *
 * Converts socks:// -> socks5:// (httpx rejects bare socks://).
 * Returns NULL for empty input.
 *
 * Port of Python utils.py:normalize_proxy_url().
 */
/* PoP: normalize_proxy_url @ utils.py:normalize_proxy_url */
static char *normalize_proxy_url(const char *proxy_url) {
    if (!proxy_url || !proxy_url[0]) return NULL;

    /* Skip leading whitespace */
    const char *p = proxy_url;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return NULL;

    /* Check for socks:// prefix */
    if (strncasecmp(p, "socks://", 8) == 0) {
        /* socks://host:port -> socks5://host:port */
        size_t rest_len = strlen(p + 8);
        char *result = (char *)malloc(9 + rest_len + 1);
        if (!result) return NULL;
        memcpy(result, "socks5://", 9);
        memcpy(result + 9, p + 8, rest_len + 1);
        return result;
    }

    /* Trim trailing whitespace and return copy */
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, p, len);
    result[len] = '\0';
    return result;
}

/**
 * Check if a hostname is excluded by the NO_PROXY environment variable.
 *
 * Simple port of Python's urllib.request.proxy_bypass_environment().
 * NO_PROXY can contain comma-separated hostnames or domains.
 * A leading dot means all subdomains (.example.com matches api.example.com).
 */
static bool is_no_proxy_host(const char *hostname) {
    if (!hostname || !hostname[0]) return false;

    const char *no_proxy = getenv("NO_PROXY");
    if (!no_proxy) no_proxy = getenv("no_proxy");
    if (!no_proxy || !no_proxy[0]) return false;

    /* Make a writable copy to strtok */
    char *copy = strdup(no_proxy);
    if (!copy) return false;

    bool excluded = false;
    const char *delim = " ,;\t";
    char *token = strtok(copy, delim);
    while (token) {
        /* Trim whitespace */
        while (*token == ' ' || *token == '\t') token++;
        size_t tlen = strlen(token);
        while (tlen > 0 && (token[tlen - 1] == ' ' || token[tlen - 1] == '\t'))
            token[--tlen] = '\0';

        if (tlen == 0) { token = strtok(NULL, delim); continue; }

        if (token[0] == '.') {
            /* Domain suffix: .example.com matches anything.example.com */
            if (strlen(hostname) >= tlen - 1) {
                const char *h_suffix = hostname + strlen(hostname) - (tlen - 1);
                if (strcasecmp(h_suffix, token + 1) == 0 &&
                    (h_suffix == hostname || h_suffix[-1] == '.')) {
                    excluded = true;
                    break;
                }
            }
        } else {
            /* Exact match */
            if (strcasecmp(hostname, token) == 0) {
                excluded = true;
                break;
            }
        }
        token = strtok(NULL, delim);
    }

    free(copy);
    return excluded;
}

/* ---------- public API ---------- */

/* Port of Python agent/process_bootstrap.py:_get_proxy_from_env(). */
/* PoP: get_proxy_from_env @ agent/process_bootstrap.py:_get_proxy_from_env */
char *get_proxy_from_env(void) {
    /* Check order: HTTPS_PROXY, HTTP_PROXY, ALL_PROXY (and lowercase variants) */
    static const char *env_keys[] = {
        "HTTPS_PROXY", "https_proxy",
        "HTTP_PROXY",  "http_proxy",
        "ALL_PROXY",   "all_proxy",
        NULL
    };

    for (int i = 0; env_keys[i]; i++) {
        const char *value = getenv(env_keys[i]);
        if (!value || !value[0]) continue;
        char *normalized = normalize_proxy_url(value);
        if (normalized) return normalized;
    }

    return NULL;
}

/* Port of Python agent/process_bootstrap.py:_get_proxy_for_base_url(). */
/* PoP: get_proxy_for_base_url @ agent/process_bootstrap.py:_get_proxy_for_base_url */
char *get_proxy_for_base_url(const char *base_url) {
    if (!base_url) return get_proxy_from_env();

    char *proxy = get_proxy_from_env();
    if (!proxy) return NULL;

    /* Extract hostname from the URL */
    char *hostname = url_extract_hostname(base_url);
    if (!hostname) {
        /* Can't determine host — return proxy anyway */
        return proxy;
    }

    /* Check NO_PROXY exclusion */
    if (is_no_proxy_host(hostname)) {
        free(proxy);
        free(hostname);
        return NULL;
    }

    free(hostname);
    return proxy;
}
