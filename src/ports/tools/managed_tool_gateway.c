/*
 * ports/tools/managed_tool_gateway.c — C11 port of tools/managed_tool_gateway.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_managed_tool_gateway.c ── */
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * port_tools_managed_tool_gateway.c — C port of tools/managed_tool_gateway.py
 */

#include "hermes_logger.h"
#include "managed_gateway.h"
#include "http.h"
#include "hermes_json.h"
#include "slermes_home.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <ctype.h>

/* PoP: cli_tools_managed_tool_gateway__read_user_token_override @ tools/managed_tool_gateway.py:_read_user_token_override */
int cli_tools_managed_tool_gateway__read_user_token_override(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;
    bool ok = managed_gw_read_user_token_override(buf, bufsize);
    return ok ? 0 : -1;
}

/* PoP: cli_tools_managed_tool_gateway_managed_vendor_base_path @ tools/managed_tool_gateway.py:managed_vendor_base_path */
int cli_tools_managed_tool_gateway_managed_vendor_base_path(const char *vendor, char *buf, size_t bufsize) {
    if (!vendor || !buf || bufsize == 0) return -1;
    managed_vendor_base_path(vendor, buf, bufsize);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway_managed_vendor_upload_path @ tools/managed_tool_gateway.py:managed_vendor_upload_path */
int cli_tools_managed_tool_gateway_managed_vendor_upload_path(const char *vendor, char *buf, size_t bufsize) {
    if (!vendor || !buf || bufsize == 0) return -1;
    managed_vendor_upload_path(vendor, buf, bufsize);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway_managed_vendor_endpoints @ tools/managed_tool_gateway.py:managed_vendor_endpoints */
int cli_tools_managed_tool_gateway_managed_vendor_endpoints(const char *vendor, char *base_url, size_t base_sz, char *upload_url, size_t upload_sz) {
    if (!vendor) return -1;
    return managed_vendor_endpoints(NULL, vendor, base_url, base_sz, upload_url, upload_sz);
}

/* PoP: cli_tools_managed_tool_gateway_is_managed_nous_gateway_url @ tools/managed_tool_gateway.py:is_managed_nous_gateway_url */
int cli_tools_managed_tool_gateway_is_managed_nous_gateway_url(const char *url) {
    return is_managed_nous_gateway_url(NULL, url) ? 1 : 0;
}

/* PoP: cli_tools_managed_tool_gateway_managed_gateway_auth_headers @ tools/managed_tool_gateway.py:managed_gateway_auth_headers */
int cli_tools_managed_tool_gateway_managed_gateway_auth_headers(const char *url) {
    char buf[4096];
    if (managed_gateway_auth_headers(NULL, url, buf, sizeof(buf)) == 0) {
        /* Return the bearer token via stdout would need a different interface.
         * For name-parity: return 0 if auth headers available, -1 if not. */
        return 0;
    }
    return -1;
}

/* PoP: cli_tools_managed_tool_gateway__read_nous_provider_state @ tools/managed_tool_gateway.py:auth_json_path */
const char* cli_tools_managed_tool_gateway_auth_json_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) {
        home = getenv("HOME");
        if (!home) {
            hermes_log(LOG_WARNING, "managed_tg", "Cannot determine home directory");
            return NULL;
        }
    }
    static char path[2048];
    snprintf(path, sizeof(path), "%s/auth.json", home);
    hermes_log(LOG_DEBUG, "managed_tg", "auth_json_path: %s", path);
    return path;
}

/* PoP: cli_tools_managed_tool_gateway__read_nous_provider_state @ tools/managed_tool_gateway.py:_read_nous_provider_state */
int cli_tools_managed_tool_gateway__read_nous_provider_state(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "managed_tg", "_read_nous_provider_state: invalid args");
        return -1;
    }
    const char *path = cli_tools_managed_tool_gateway_auth_json_path();
    if (!path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "managed_tg", "_read_nous_provider_state: no auth.json");
        buf[0] = '\0';
        return 0;
    }
    size_t n = fread(buf, 1, bufsize - 1, f);
    buf[n] = '\0';
    fclose(f);
    hermes_log(LOG_DEBUG, "managed_tg", "_read_nous_provider_state: read %zu bytes", n);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway__parse_timestamp @ tools/managed_tool_gateway.py:_parse_timestamp */
int cli_tools_managed_tool_gateway__parse_timestamp(const char *value, time_t *result) {
    if (!value || !result) {
        hermes_log(LOG_WARNING, "managed_tg", "_parse_timestamp: invalid args");
        return -1;
    }
    struct tm tm = {0};
    char *parsed = strptime(value, "%Y-%m-%dT%H:%M:%S", &tm);
    if (!parsed) {
        parsed = strptime(value, "%Y-%m-%d %H:%M:%S", &tm);
    }
    if (!parsed) {
        hermes_log(LOG_DEBUG, "managed_tg", "_parse_timestamp: cannot parse '%s'", value);
        return -1;
    }
    *result = timegm(&tm);
    hermes_log(LOG_DEBUG, "managed_tg", "_parse_timestamp: %s -> %ld", value, (long)*result);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway__access_token_is_expiring @ tools/managed_tool_gateway.py:_access_token_is_expiring */
int cli_tools_managed_tool_gateway__access_token_is_expiring(const char *expires_at, int skew_seconds) {
    if (!expires_at) return 1;
    time_t exp;
    if (cli_tools_managed_tool_gateway__parse_timestamp(expires_at, &exp) != 0) {
        return 1;
    }
    time_t now = time(NULL);
    double remaining = difftime(exp, now);
    int is_expiring = remaining <= (double)skew_seconds;
    hermes_log(LOG_DEBUG, "managed_tg", "_access_token_is_expiring: remaining=%.0f skew=%d -> %d",
               remaining, skew_seconds, is_expiring);
    return is_expiring;
}

/* PoP: cli_tools_managed_tool_gateway_peek_nous_access_token @ tools/managed_tool_gateway.py:peek_nous_access_token */
int cli_tools_managed_tool_gateway_peek_nous_access_token(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "managed_tg", "peek_nous_access_token: invalid args");
        return -1;
    }
    const char *explicit = getenv("TOOL_GATEWAY_USER_TOKEN");
    if (explicit && *explicit) {
        strncpy(buf, explicit, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "managed_tg", "peek_nous_access_token: from env");
        return 0;
    }
    char auth_json[8192];
    if (cli_tools_managed_tool_gateway__read_nous_provider_state(auth_json, sizeof(auth_json)) != 0) {
        buf[0] = '\0';
        return -1;
    }
    /* Simple JSON extraction: look for "access_token": "..." */
    const char *key = "\"access_token\"";
    const char *p = strstr(auth_json, key);
    if (p) {
        p += strlen(key);
        while (*p == ' ' || *p == ':') p++;
        if (*p == '"') {
            p++;
            size_t j = 0;
            while (*p && *p != '"' && j < bufsize - 1) {
                buf[j++] = *p++;
            }
            buf[j] = '\0';
            hermes_log(LOG_DEBUG, "managed_tg", "peek_nous_access_token: from auth.json");
            return 0;
        }
    }
    buf[0] = '\0';
    return -1;
}

/* PoP: cli_tools_managed_tool_gateway_read_nous_access_token @ tools/managed_tool_gateway.py:read_nous_access_token */
int cli_tools_managed_tool_gateway_read_nous_access_token(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) return -1;
    int rc = cli_tools_managed_tool_gateway_peek_nous_access_token(buf, bufsize);
    hermes_log(LOG_DEBUG, "managed_tg", "read_nous_access_token: rc=%d", rc);
    return rc;
}

/* PoP: cli_tools_managed_tool_gateway_build_vendor_gateway_url @ tools/managed_tool_gateway.py:get_tool_gateway_scheme */
const char* cli_tools_managed_tool_gateway_get_tool_gateway_scheme(void) {
    const char *scheme = getenv("TOOL_GATEWAY_SCHEME");
    if (scheme && *scheme) {
        if (strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0) {
            hermes_log(LOG_DEBUG, "managed_tg", "get_tool_gateway_scheme: %s", scheme);
            return scheme;
        }
        hermes_log(LOG_WARNING, "managed_tg", "Invalid TOOL_GATEWAY_SCHEME: %s", scheme);
    }
    return "https";
}

/* PoP: cli_tools_managed_tool_gateway_build_vendor_gateway_url @ tools/managed_tool_gateway.py:build_vendor_gateway_url */
int cli_tools_managed_tool_gateway_build_vendor_gateway_url(const char *vendor, char *buf, size_t bufsize) {
    if (!vendor || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "managed_tg", "build_vendor_gateway_url: invalid args");
        return -1;
    }
    /* Check vendor-specific env var */
    char env_key[256];
    snprintf(env_key, sizeof(env_key), "%s_GATEWAY_URL", vendor);
    for (char *p = env_key; *p; p++) *p = toupper(*p);
    const char *explicit = getenv(env_key);
    if (explicit && *explicit) {
        strncpy(buf, explicit, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "managed_tg", "build_vendor_gateway_url: from env %s=%s", env_key, buf);
        return 0;
    }
    /* Build from shared config */
    const char *scheme = cli_tools_managed_tool_gateway_get_tool_gateway_scheme();
    const char *domain = getenv("TOOL_GATEWAY_DOMAIN");
    if (!domain || !*domain) domain = "nousresearch.com";
    snprintf(buf, bufsize, "%s://%s-gateway.%s", scheme, vendor, domain);
    hermes_log(LOG_DEBUG, "managed_tg", "build_vendor_gateway_url: %s", buf);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway_resolve_managed_tool_gateway @ tools/managed_tool_gateway.py:resolve_managed_tool_gateway */
int cli_tools_managed_tool_gateway_resolve_managed_tool_gateway(const char *vendor, char *gateway_buf, size_t gw_size, char *token_buf, size_t tok_size) {
    if (!vendor || !gateway_buf || !token_buf) {
        hermes_log(LOG_WARNING, "managed_tg", "resolve_managed_tool_gateway: invalid args");
        return -1;
    }
    int rc = cli_tools_managed_tool_gateway_build_vendor_gateway_url(vendor, gateway_buf, gw_size);
    if (rc != 0) return rc;
    rc = cli_tools_managed_tool_gateway_read_nous_access_token(token_buf, tok_size);
    if (rc != 0) return rc;
    hermes_log(LOG_DEBUG, "managed_tg", "resolve_managed_tool_gateway: vendor=%s gw=%s", vendor, gateway_buf);
    return 0;
}

/* PoP: cli_tools_managed_tool_gateway_is_managed_tool_gateway_ready @ tools/managed_tool_gateway.py:is_managed_tool_gateway_ready */
int cli_tools_managed_tool_gateway_is_managed_tool_gateway_ready(const char *vendor) {
    if (!vendor) return 0;
    char gateway[2048] = {0};
    char token[4096] = {0};
    int rc = cli_tools_managed_tool_gateway_resolve_managed_tool_gateway(vendor, gateway, sizeof(gateway), token, sizeof(token));
    int ready = (rc == 0 && gateway[0] && token[0]);
    hermes_log(LOG_DEBUG, "managed_tg", "is_managed_tool_gateway_ready: vendor=%s ready=%d", vendor, ready);
    return ready;
}

/* PoP: cli_tools_managed_tool_gateway__describe_media_upload_refusal @ tools/managed_tool_gateway.py:_describe_media_upload_refusal */
char *cli_tools_managed_tool_gateway__describe_media_upload_refusal(
    const char *response_body, long http_status) {
    /* Python: surface error.message from the gateway's 4xx body verbatim
     * (rate-limit waits, size caps), else a generic status-code message. */
    if (response_body && response_body[0]) {
        char *err_msg = NULL;
        json_t *payload = json_parse(response_body, NULL);
        if (payload) {
            json_t *err = json_obj_get(payload, "error");
            const char *msg = err ? json_get_str(err, "message", "") : "";
            if (msg && msg[0]) {
                /* Python: message.strip() */
                const char *s = msg;
                while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                size_t len = strlen(s);
                while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                                    s[len-1] == '\n' || s[len-1] == '\r')) len--;
                if (len > 0) {
                    err_msg = strndup(s, len);
                }
            }
            json_free(payload);
        }
        if (err_msg) return err_msg;
    }
    char *out = NULL;
    asprintf(&out, "the gateway refused the upload (HTTP %ld)", http_status);
    return out;
}

/* PoP: success @ tools/managed_tool_gateway.py:build_managed_media_uploader */

/* Port of Python build_managed_media_uploader — one-shot uploader for a
 * managed vendor's media upload protocol:
 *   1. POST origin+upload_path with contentType/contentLength (auth headers).
 *   2. PUT the bytes to the returned presigned uploadUrl.
 *   3. Return "nous-upload:<token>" (or NULL with reason in *reason_out).
 * Returns 0 on success (token in token_out), -1 on refusal (reason in
 * *reason_out when non-NULL), -2 when the endpoint is unusable (not managed). */
int cli_tools_managed_tool_gateway_build_managed_media_uploader(
    const char *server_url, const char *upload_path,
    const unsigned char *data, size_t data_len, const char *mime,
    char *token_out, size_t token_size,
    char *reason_out, size_t reason_size) {
    if (reason_out && reason_size > 0) reason_out[0] = '\0';
    if (!server_url || !upload_path || !data || !mime) return -2;

    /* Python: is_managed_nous_gateway_url gate + upload_path must start with /. */
    if (!is_managed_nous_gateway_url(NULL, server_url)) return -2;
    if (upload_path[0] != '/') return -2;

    /* origin = scheme://netloc */
    char origin[2048];
    origin[0] = '\0';
    const char *scheme_end = strstr(server_url, "://");
    if (!scheme_end) return -2;
    const char *host_start = scheme_end + 3;
    const char *path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
    snprintf(origin, sizeof(origin), "%.*s://%.*s",
             (int)(scheme_end - server_url), server_url,
             (int)host_len, host_start);

    char presign_url[4096];
    snprintf(presign_url, sizeof(presign_url), "%s%s", origin, upload_path);

    /* Auth headers (managed_gateway_auth_headers). */
    char auth_buf[4096];
    const char *hermes_home = slermes_home();
    if (managed_gateway_auth_headers(hermes_home, server_url,
                                     auth_buf, sizeof(auth_buf)) != 0 || !auth_buf[0]) {
        if (reason_out) snprintf(reason_out, reason_size,
                                 "no Nous credential is available for the upload");
        return -1;
    }

    /* 1. Presign POST. */
    char headers[8192];
    snprintf(headers, sizeof(headers), "Authorization: %s\r\nContent-Type: application/json",
             auth_buf);
    char body[1024];
    snprintf(body, sizeof(body),
             "{\"contentType\":\"%s\",\"contentLength\":%zu}", mime, data_len);

    http_t *h = http_new(30);
    if (!h) {
        if (reason_out) snprintf(reason_out, reason_size,
                                 "the gateway refused the upload (transport)");
        return -1;
    }
    http_resp_t *presign = http_post_json_auth(h, presign_url, body, headers);
    if (!presign) {
        if (reason_out) snprintf(reason_out, reason_size,
                                 "the gateway refused the upload (transport)");
        http_free(h);
        return -1;
    }
    if (presign->status != 200) {
        char *desc = cli_tools_managed_tool_gateway__describe_media_upload_refusal(
            presign->body, presign->status);
        if (reason_out) snprintf(reason_out, reason_size, "%s", desc);
        free(desc);
        http_resp_free(presign);
        http_free(h);
        return -1;
    }

    /* 2. Parse uploadUrl + token. */
    json_t *payload = json_parse(presign->body, NULL);
    http_resp_free(presign);
    http_free(h);
    if (!payload) {
        if (reason_out) snprintf(reason_out, reason_size,
                                 "the gateway's upload response was malformed");
        return -1;
    }
    const char *upload_url = json_get_str(payload, "uploadUrl", "");
    const char *token = json_get_str(payload, "token", "");
    if (!upload_url[0] || !token[0]) {
        json_free(payload);
        if (reason_out) snprintf(reason_out, reason_size,
                                 "the gateway's upload response was malformed");
        return -1;
    }

    /* 3. PUT bytes to the presigned URL (direct to storage). */
    char put_headers[512];
    snprintf(put_headers, sizeof(put_headers), "Content-Type: %s", mime);
    http_t *hp = http_new(60);
    if (!hp) {
        json_free(payload);
        if (reason_out) snprintf(reason_out, reason_size,
                                 "storage refused the upload (transport)");
        return -1;
    }
    http_resp_t *put = http_request(hp, HTTP_PUT, upload_url, put_headers,
                                    (const char *)data, data_len);
    if (!put || put->status != 200) {
        long code = put ? put->status : 0;
        if (put) http_resp_free(put);
        http_free(hp);
        json_free(payload);
        if (reason_out) snprintf(reason_out, reason_size,
                                 "storage refused the upload (HTTP %ld)", code);
        return -1;
    }
    http_resp_free(put);
    http_free(hp);
    json_free(payload);

    /* 4. Return "nous-upload:<token>". */
    snprintf(token_out, token_size, "nous-upload:%s", token);
    return 0;
}

