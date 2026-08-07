/*
 * managed_gateway.c — Generic managed-tool gateway helpers for
 * Nous-hosted vendor passthroughs.
 * Port of Python tools/managed_tool_gateway.py.
 */

#include "managed_gateway.h"
#include "tool_backend.h"  /* managed_nous_tools_enabled() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

/* ─── Constants ─────────────────────────────────────────── */

#define DEFAULT_GATEWAY_DOMAIN "nousresearch.com"
#define DEFAULT_GATEWAY_SCHEME "https"
#define ACCESS_TOKEN_REFRESH_SKEW 120  /* seconds */

/* ─── Internal helpers ──────────────────────────────────── */

/* Case-insensitive comparison */
static int strcasecmp_safe(const char *a, const char *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Read file content into malloc'd buffer. Caller must free. */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Simple JSON field extractor — finds "key": "value" or "key": value */
static int json_extract_string(const char *json, const char *key,
                                char *buf, size_t sz)
{
    if (!json || !key || !buf || sz == 0) return 0;

    /* Find "key": */
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);

    /* Skip whitespace and colon */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != ':') return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    /* Handle string or non-string value */
    if (*p == '"') {
        p++; /* skip opening quote */
        size_t pos = 0;
        while (*p && *p != '"' && pos < sz - 1) {
            if (*p == '\\' && *(p+1)) {
                p++; /* skip escape */
                if (pos < sz - 1) buf[pos++] = *p;
            } else {
                buf[pos++] = *p;
            }
            p++;
        }
        buf[pos] = '\0';
        return pos > 0 ? 1 : 0;
    }

    /* Non-string value (number, bool) */
    size_t pos = 0;
    while (*p && *p != ',' && *p != '}' && *p != '\n' && pos < sz - 1) {
        buf[pos++] = *p++;
    }
    buf[pos] = '\0';
    return pos > 0 ? 1 : 0;
}

/* ─── Auth JSON path ─────────────────────────────────────── */

void managed_gw_auth_json_path(const char *hermes_home,
                                char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    if (hermes_home && hermes_home[0]) {
        snprintf(buf, sz, "%s/auth.json", hermes_home);
    } else {
        const char *home = getenv("HOME");
        if (home)
            snprintf(buf, sz, "%s/.slermes/auth.json", home);
        else
            snprintf(buf, sz, "auth.json");
    }
}

/* ─── Access token reader ───────────────────────────────── */

bool managed_gw_read_access_token(const char *hermes_home,
                                   char *buf, size_t sz)
{
    if (!buf || sz == 0) return false;
    buf[0] = '\0';

    /* Priority 1: TOOL_GATEWAY_USER_TOKEN env var */
    const char *explicit = getenv("TOOL_GATEWAY_USER_TOKEN");
    if (explicit && explicit[0]) {
        strncpy(buf, explicit, sz - 1);
        buf[sz - 1] = '\0';
        return true;
    }

    /* Read auth.json */
    char path[1024];
    managed_gw_auth_json_path(hermes_home, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return false;

    char *json = read_file(path);
    if (!json) return false;

    /* Extract access_token */
    char token[GW_TOKEN_MAX];
    if (!json_extract_string(json, "access_token", token, sizeof(token))) {
        /* Try nested: providers -> nous -> access_token */
        const char *providers = strstr(json, "\"providers\"");
        if (providers) {
            const char *nous = strstr(providers, "\"nous\"");
            if (nous) {
                json_extract_string(nous, "access_token", token, sizeof(token));
            }
        }
    }

    /* Extract expires_at for skew check */
    char expires_str[64];
    bool has_expires = json_extract_string(json, "expires_at",
                                            expires_str, sizeof(expires_str));

    free(json);

    if (!token[0]) return false;

    /* Check if token is expiring */
    if (has_expires && expires_str[0]) {
        /* Simple timestamp parse (ISO-8601 or Unix timestamp) */
        time_t expires_at = 0;
        char *end = NULL;
        expires_at = (time_t)strtoll(expires_str, &end, 10);
        if (end == expires_str || expires_at <= 0) {
            /* Not a number — try parsing as ISO-8601 date */
            /* Simple heuristic: token is valid */
        } else {
            time_t now = time(NULL);
            if (now + ACCESS_TOKEN_REFRESH_SKEW >= expires_at) {
                /* Token is expiring — still return it as fallback */
                strncpy(buf, token, sz - 1);
                buf[sz - 1] = '\0';
                return true;
            }
        }
    }

    strncpy(buf, token, sz - 1);
    buf[sz - 1] = '\0';
    return true;
}

/* ─── Scheme ────────────────────────────────────────────── */

const char *managed_gw_get_scheme(void)
{
    const char *scheme = getenv("TOOL_GATEWAY_SCHEME");
    if (scheme) {
        if (strcasecmp_safe(scheme, "http") == 0 ||
            strcasecmp_safe(scheme, "https") == 0)
            return scheme;
    }
    return DEFAULT_GATEWAY_SCHEME;
}

/* ─── URL builder ───────────────────────────────────────── */

void managed_gw_build_url(const char *vendor, char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    buf[0] = '\0';
    if (!vendor || !vendor[0]) return;

    /* Priority 1: <VENDOR>_GATEWAY_URL env var */
    char env_name[128];
    size_t i;
    for (i = 0; vendor[i] && i < sizeof(env_name) - 20; i++)
        env_name[i] = (char)toupper((unsigned char)vendor[i]);
    env_name[i] = '\0';
    strcat(env_name, "_GATEWAY_URL");

    const char *explicit = getenv(env_name);
    if (explicit && explicit[0]) {
        strncpy(buf, explicit, sz - 1);
        buf[sz - 1] = '\0';
        return;
    }

    /* Priority 2: TOOL_GATEWAY_DOMAIN env var */
    const char *domain = getenv("TOOL_GATEWAY_DOMAIN");
    if (!domain || !domain[0])
        domain = DEFAULT_GATEWAY_DOMAIN;

    const char *scheme = managed_gw_get_scheme();
    snprintf(buf, sz, "%s://%s-gateway.%s", scheme, vendor, domain);
}

/* ─── Resolver ──────────────────────────────────────────── */

bool managed_gw_resolve(const char *hermes_home,
                         const char *vendor,
                         managed_gateway_config_t *config)
{
    if (!config) return false;

    memset(config, 0, sizeof(*config));

    /* Check managed tools are enabled */
    if (!managed_nous_tools_enabled())
        return false;

    if (!vendor || !vendor[0])
        return false;

    strncpy(config->vendor, vendor, sizeof(config->vendor) - 1);

    /* Build gateway URL */
    managed_gw_build_url(vendor, config->gateway_origin,
                         sizeof(config->gateway_origin));
    if (!config->gateway_origin[0])
        return false;

    /* Read Nous access token */
    if (!managed_gw_read_access_token(hermes_home,
                                       config->nous_user_token,
                                       sizeof(config->nous_user_token)))
        return false;

    config->managed_mode = true;
    return true;
}

/* ─── URL parsing helper (urlsplit-ish) ────────────────────── */
/* Split url into scheme://netloc/path. Fills netloc_buf and path_buf.
 * Returns 0 on success. */
static int gw_split_url(const char *url, char *scheme_buf, size_t scheme_sz,
                         char *netloc_buf, size_t netloc_sz,
                         char *path_buf, size_t path_sz)
{
    if (!url || !*url) return -1;
    /* Find scheme */
    const char *colon = strstr(url, "://");
    if (!colon) return -1;
    size_t scheme_len = (size_t)(colon - url);
    if (scheme_len >= scheme_sz) return -1;
    memcpy(scheme_buf, url, scheme_len);
    scheme_buf[scheme_len] = '\0';
    const char *rest = colon + 3;
    /* netloc is up to first '/' or end */
    const char *slash = strchr(rest, '/');
    const char *end = slash ? slash : rest + strlen(rest);
    size_t nl_len = (size_t)(end - rest);
    if (nl_len >= netloc_sz) return -1;
    memcpy(netloc_buf, rest, nl_len);
    netloc_buf[nl_len] = '\0';
    if (slash) {
        strncpy(path_buf, slash, path_sz - 1);
        path_buf[path_sz - 1] = '\0';
    } else {
        path_buf[0] = '\0';
    }
    return 0;
}

/* ─── Readiness check ───────────────────────────────────── */

bool managed_gw_is_ready(const char *hermes_home, const char *vendor)
{
    managed_gateway_config_t cfg;
    return managed_gw_resolve(hermes_home, vendor, &cfg);
}

/* ─── Vendor paths ───────────────────────────────────────── */

/* PoP: managed_vendor_base_path @ tools/managed_tool_gateway.py:managed_vendor_base_path
 * Base path for a managed vendor's REST routes on the gateway host.
 * Python: return f"/api/{vendor}". Returns "/api/<vendor>". */
void managed_vendor_base_path(const char *vendor, char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    if (!vendor || !vendor[0]) { buf[0] = '\0'; return; }
    snprintf(buf, sz, "/api/%s", vendor);
}

/* PoP: managed_vendor_upload_path @ tools/managed_tool_gateway.py:managed_vendor_upload_path
 * Media upload endpoint for a managed vendor.
 * Python: return f"/api/uploads/{vendor}". */
void managed_vendor_upload_path(const char *vendor, char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    if (!vendor || !vendor[0]) { buf[0] = '\0'; return; }
    snprintf(buf, sz, "/api/uploads/%s", vendor);
}

/* PoP: managed_vendor_endpoints @ tools/managed_tool_gateway.py:managed_vendor_endpoints
 * Resolve absolute URLs for a managed vendor. Returns 0 on success, -1
 * when no origin could be resolved. */
int managed_vendor_endpoints(const char *hermes_home, const char *vendor,
                              char *base_url, size_t base_sz,
                              char *upload_url, size_t upload_sz)
{
    managed_gateway_config_t cfg;
    if (!managed_gw_resolve(hermes_home, vendor, &cfg)) return -1;
    char base_path[256];
    managed_vendor_base_path(vendor, base_path, sizeof(base_path));
    char upload_path[256];
    managed_vendor_upload_path(vendor, upload_path, sizeof(upload_path));
    snprintf(base_url, base_sz, "%s%s", cfg.gateway_origin, base_path);
    snprintf(upload_url, upload_sz, "%s%s", cfg.gateway_origin, upload_path);
    return 0;
}

/* PoP: is_managed_nous_gateway_url @ tools/managed_tool_gateway.py:is_managed_nous_gateway_url
 * True when url is on the Nous tool-gateway origin this client builds. */
bool is_managed_nous_gateway_url(const char *hermes_home,
                                  const char *url)
{
    if (!url || !*url || !hermes_home) return false;
    char expected_host[GW_URL_MAX];
    managed_gw_build_url("tool", expected_host, sizeof(expected_host));
    char exp_scheme[16], exp_netloc[GW_URL_MAX];
    if (gw_split_url(expected_host, exp_scheme, sizeof(exp_scheme),
                     exp_netloc, sizeof(exp_netloc),
                     expected_host, sizeof(expected_host)) != 0) return false;
    char act_scheme[16], act_netloc[GW_URL_MAX], act_path[GW_URL_MAX];
    if (gw_split_url(url, act_scheme, sizeof(act_scheme),
                     act_netloc, sizeof(act_netloc),
                     act_path, sizeof(act_path)) != 0) return false;
    return strcasecmp_safe(act_scheme, exp_scheme) == 0 &&
           strcasecmp_safe(act_netloc, exp_netloc) == 0;
}

/* PoP: managed_gateway_auth_headers @ tools/managed_tool_gateway.py:managed_gateway_auth_headers
 * Live auth headers for a managed gateway URL. Returns 0 and sets
 * "Bearer <token>" on out_buf when managed + token available.
 * Returns -1 when no token (caller should report "sign in"). */
int managed_gateway_auth_headers(const char *hermes_home,
                                  const char *url,
                                  char *out_buf, size_t sz)
{
    if (!hermes_home || !url || !out_buf || sz == 0) return -1;
    if (!is_managed_nous_gateway_url(hermes_home, url)) return -1;
    char token[GW_TOKEN_MAX];
    if (!managed_gw_read_access_token(hermes_home, token, sizeof(token)) || !token[0])
        return -1;
    snprintf(out_buf, sz, "Bearer %s", token);
    return 0;
}

/* ─── Vendor path ───────────────────────────────────────── */

/* PoP: _read_user_token_override @ tools/managed_tool_gateway.py:_read_user_token_override
 * Read the TOOL_GATEWAY_USER_TOKEN env override.
 * Honors the secret scope when the SLERMES_SECRET_SCOPE env is set. */
bool managed_gw_read_user_token_override(char *buf, size_t sz)
{
    if (!buf || sz == 0) return false;
    /* Try secret scope first (if configured) */
    const char *scope = getenv("SLERMES_SECRET_SCOPE");
    if (scope && scope[0]) {
        char key[256];
        snprintf(key, sizeof(key), "%s/TOOL_GATEWAY_USER_TOKEN", scope);
        /* Simple: try the scope path as a file fallback */
        FILE *f = fopen(key, "r");
        if (f) {
            if (fgets(buf, (int)sz, f)) {
                /* strip trailing whitespace/newline */
                size_t n = strlen(buf);
                while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || isspace((unsigned char)buf[n-1])))
                    buf[--n] = '\0';
                fclose(f);
                if (buf[0]) return true;
            }
            fclose(f);
        }
    }
    /* Fall back to env */
    const char *explicit = getenv("TOOL_GATEWAY_USER_TOKEN");
    if (explicit && explicit[0]) {
        const char *p = explicit;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t n = strlen(p);
        while (n > 0 && isspace((unsigned char)p[n-1])) n--;
        if (n > 0 && n < sz) {
            memcpy(buf, p, n); buf[n] = '\0';
            return true;
        }
    }
    return false;
}
