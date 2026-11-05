/*
 * port_anthropic_credentials.c — Port of Python: agent/anthropic_credentials.py
 * (pure classification core).
 */

#include "port_anthropic_credentials.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* urlparse-ish hostname: strip scheme, userinfo, port, path; lowercase;
 * drop trailing dots. */
/* PoP: anc_base_url_hostname @ agent/anthropic_credentials.py:base_url_hostname */
char *anc_base_url_hostname(const char *base_url)
{
    if (!base_url) return dup_str("");
    const char *p = base_url;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t n = strlen(p);
    while (n > 0 && isspace((unsigned char)p[n - 1])) n--;
    if (n == 0) return dup_str("");

    char *buf = (char *)malloc(n + 1);
    if (!buf) return NULL;
    {
        const char *scheme = strstr(p, "://");
        snprintf(buf, n + 1, "%s", scheme ? scheme + 3 : p);
    }

    /* authority ends at the first '/', '?', '#' */
    size_t alen = strcspn(buf, "/?#");
    buf[alen] = '\0';

    /* userinfo: last '@' separates credentials from host */
    char *host = buf;
    char *at = strrchr(buf, '@');
    if (at) host = at + 1;

    /* IPv6 literal in brackets keeps its colons but urlparse drops the
     * brackets from .hostname, so strip them here too. */
    if (*host == '[') {
        char *close = strchr(host, ']');
        if (close) *close = '\0';
        memmove(host, host + 1, strlen(host + 1) + 1);
    } else {
        char *colon = strrchr(host, ':');
        if (colon) *colon = '\0';
    }

    size_t hlen = strlen(host);
    while (hlen > 0 && host[hlen - 1] == '.') host[--hlen] = '\0';
    for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);

    char *out = dup_str(host);
    free(buf);
    return out;
}

/* PoP: anc_is_oauth_token @ agent/anthropic_credentials.py:_is_oauth_token */
int anc_is_oauth_token(const char *key)
{
    if (!key || !*key) return 0;
    static const char *const API_KEY_FAMILIES[] = {
        "sk-ant-api", "sk-ant-usr", "sk-ant-svc", "sk-ant-admin",
    };
    for (size_t i = 0; i < sizeof(API_KEY_FAMILIES) / sizeof(API_KEY_FAMILIES[0]); i++)
        if (strncmp(key, API_KEY_FAMILIES[i], strlen(API_KEY_FAMILIES[i])) == 0)
            return 0;
    return strncmp(key, "sk-ant-", 7) == 0 ||
           strncmp(key, "eyJ", 3) == 0 ||
           strncmp(key, "cc-", 3) == 0;
}

/* PoP: anc_is_native_host @ agent/anthropic_credentials.py:is_native_anthropic_host */
int anc_is_native_host(const char *base_url)
{
    char *host = anc_base_url_hostname(base_url);
    if (!host) return 0;
    int native = (host[0] == '\0') || strcmp(host, "api.anthropic.com") == 0;
    free(host);
    return native;
}

/* PoP: anc_route_is_oauth @ agent/anthropic_credentials.py:anthropic_route_is_oauth */
int anc_route_is_oauth(const char *base_url, const char *credential,
                       const char *provider)
{
    int provider_is_anthropic = 0;
    if (provider) {
        while (*provider && isspace((unsigned char)*provider)) provider++;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", provider);
        size_t n = strlen(buf);
        while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
        for (char *q = buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        provider_is_anthropic = strcmp(buf, "anthropic") == 0;
    }
    if (!anc_is_native_host(base_url) && !provider_is_anthropic) return 0;
    return anc_is_oauth_token(credential);
}

/* PoP: anc_claude_token_valid @ agent/anthropic_credentials.py:is_claude_code_token_valid */
int anc_claude_token_valid(const char *creds_json, double now_ms)
{
    if (!creds_json) return 0;
    json_t *doc = json_parse(creds_json, NULL);
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return 0;
    }
    json_t *expires = json_obj_get(doc, "expiresAt");
    int valid;
    double expires_at = 0.0;
    if (expires && expires->type == JSON_NUMBER) expires_at = expires->num_val;
    if (expires_at != 0.0) {
        valid = now_ms < (expires_at - 60000.0);
    } else {
        const char *tok = json_get_str(doc, "accessToken", NULL);
        valid = tok && *tok;
    }
    json_free(doc);
    return valid;
}

/* PoP: anc_credentials_path @ agent/anthropic_credentials.py:claude_code_credentials_path */
char *anc_credentials_path(const char *claude_config_dir, const char *home)
{
    char root[1024];
    const char *override = claude_config_dir;
    while (override && *override && isspace((unsigned char)*override)) override++;
    if (override && *override) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s", override);
        size_t n = strlen(buf);
        while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
        /* Path.expanduser(): a leading ~ or ~/ uses the home directory */
        if (buf[0] == '~') {
            if (buf[1] == '\0' || buf[1] == '/' || buf[1] == '\\')
                snprintf(root, sizeof(root), "%s%s", home ? home : "", buf + 1);
            else
                snprintf(root, sizeof(root), "%s", buf);
        } else {
            snprintf(root, sizeof(root), "%s", buf);
        }
    } else {
        snprintf(root, sizeof(root), "%s/.claude", home ? home : "");
    }
    size_t cap = strlen(root) + 32;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "%s/.credentials.json", root);
    return out;
}
