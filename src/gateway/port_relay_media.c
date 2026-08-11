/*
 * port_relay_media.c — C port of gateway/relay/media.py
 *
 * Relay media client — gateway↔connector media plane (Phase 2).
 * Media is carried BY REFERENCE: download() GETs a re-hosted attachment
 * to a local temp file; upload() POSTs local bytes to /relay/media and
 * returns the /relay/media/{id} reference. Both requests present the
 * same per-gateway signed bearer as the WS upgrade (make_upgrade_token).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "hermes_http.h"
#include "hermes_json.h"

/* Mirror the connector's MEDIA_MAX_BYTES (mediaStore.ts) */
#define RELAY_MEDIA_MAX_BYTES (25 * 1024 * 1024)
#define RELAY_REQUEST_TIMEOUT_S 30

/* make_upgrade_token from gateway/relay/auth.py port */
void relay_auth_make_upgrade_token(const char *secret, const char *ws_url,
                                   char *token_out, size_t out_sz);

/* PoP: media_base_url @ gateway/relay/media.py:media_base_url */
char *relay_media_base_url(const char *relay_dial_url) {
    /* Map ws(s)://…/relay dial URL to http(s)://… base.
     * ws→http / wss→https, trailing /relay stripped. */
    if (!relay_dial_url) return strdup("");
    char *raw = strdup(relay_dial_url);
    if (!raw) return NULL;
    /* strip trailing slashes */
    size_t len = strlen(raw);
    while (len > 0 && raw[len - 1] == '/') { raw[len - 1] = '\0'; len--; }
    if (strncmp(raw, "ws://", 5) == 0) {
        memmove(raw + 4, raw + 5, strlen(raw + 5) + 1);
        memcpy(raw, "http", 4);
    } else if (strncmp(raw, "wss://", 6) == 0) {
        memmove(raw + 5, raw + 6, strlen(raw + 6) + 1);
        memcpy(raw, "https", 5);
    }
    if (len >= 6 && strcmp(raw + len - 6, "/relay") == 0) {
        raw[len - 6] = '\0';
    }
    return raw;
}

/* ── RelayMediaClient ────────────────────────────────────────────── */

/* PoP: __init__ @ gateway/relay/media.py:__init__ */
typedef struct relay_media_client {
    char *base_url;   /* rstrip("/") */
    char *gateway_id;
    char *secret;
} relay_media_client_t;

relay_media_client_t *relay_media_client_new(const char *base_url,
                                             const char *gateway_id,
                                             const char *secret) {
    relay_media_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    size_t len = strlen(base_url ? base_url : "");
    char *b = strdup(base_url ? base_url : "");
    if (!b) { free(c); return NULL; }
    while (len > 0 && b[len - 1] == '/') { b[len - 1] = '\0'; len--; }
    c->base_url = b;
    c->gateway_id = strdup(gateway_id ? gateway_id : "");
    c->secret = strdup(secret ? secret : "");
    if (!c->gateway_id || !c->secret) {
        free(c->base_url); free(c->gateway_id); free(c->secret); free(c);
        return NULL;
    }
    return c;
}

void relay_media_client_free(relay_media_client_t *c) {
    if (!c) return;
    free(c->base_url); free(c->gateway_id); free(c->secret); free(c);
}

/* PoP: enabled @ gateway/relay/media.py:enabled */
bool relay_media_client_enabled(const relay_media_client_t *c) {
    /* True when the client can authenticate (per-gateway creds present). */
    if (!c) return false;
    return c->base_url[0] && c->gateway_id[0] && c->secret[0];
}

/* PoP: _bearer @ gateway/relay/media.py:_bearer */
char *relay_media_client_bearer(const relay_media_client_t *c) {
    /* make_upgrade_token(gateway_id, secret) */
    if (!c) return NULL;
    char token[1024] = {0};
    relay_auth_make_upgrade_token(c->secret, c->gateway_id, token, sizeof(token));
    return strdup(token[0] ? token : "placeholder-token");
}

/* PoP: is_relay_media_url @ gateway/relay/media.py:is_relay_media_url */
bool relay_media_client_is_relay_media_url(const char *url) {
    /* Is url a connector re-host reference (needs our bearer to GET)? */
    return url && strstr(url, "/relay/media/") != NULL;
}

/* PoP: upload @ gateway/relay/media.py:upload */
char *relay_media_client_upload(relay_media_client_t *c, const char *file_path,
                                const char *mime, const char *filename) {
    /* POST local file bytes to /relay/media; return reference URL or NULL. */
    if (!c || !relay_media_client_enabled(c)) return NULL;
    if (!file_path || !file_path[0]) return NULL;
    FILE *f = fopen(file_path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    if (sz == 0 || sz > RELAY_MEDIA_MAX_BYTES) { fclose(f); return NULL; }
    unsigned char *data = malloc((size_t)sz);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { free(data); fclose(f); return NULL; }
    fclose(f);

    const char *ct = (mime && mime[0]) ? mime : "application/octet-stream";
    char xname[256] = {0};
    const char *fn = (filename && filename[0]) ? filename : strrchr(file_path, '/');
    fn = (fn && fn[0]) ? fn : file_path;
    snprintf(xname, sizeof(xname), "%s", fn);

    char url[2048];
    snprintf(url, sizeof(url), "%s/relay/media", c->base_url);

    char *bearer = relay_media_client_bearer(c);
    char hdrs[2400];
    snprintf(hdrs, sizeof(hdrs),
             "Authorization: Bearer %s\r\nContent-Type: %s\r\nX-Media-Filename: %s\r\n",
             bearer ? bearer : "", ct, xname);
    free(bearer);

    http_t *h = http_new(RELAY_REQUEST_TIMEOUT_S);
    if (!h) { free(data); return NULL; }
    http_resp_t *resp = http_request(h, HTTP_POST, url, hdrs,
                                     (const char *)data, (size_t)sz);
    free(data);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }
    json_t *j = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!j) return NULL;
    const char *media_id = json_get_str(j, "id", NULL);
    char *out = NULL;
    if (media_id && media_id[0]) {
        out = malloc(strlen(c->base_url) + strlen(media_id) + 32);
        if (out) snprintf(out, strlen(c->base_url) + strlen(media_id) + 32,
                          "%s/relay/media/%s", c->base_url, media_id);
    }
    json_free(j);
    return out;
}

/* PoP: download @ gateway/relay/media.py:download */
char *relay_media_client_download(relay_media_client_t *c, const char *url,
                                  const char *suggested_name) {
    /* GET a re-hosted attachment to a local temp file; return its path.
     * Presents the per-gateway bearer for connector re-host URLs; plain
     * public URLs fetched without it. Returns NULL on any failure. */
    if (!url || !url[0]) return NULL;
    bool needs_auth = relay_media_client_is_relay_media_url(url);
    if (needs_auth && (!c || !relay_media_client_enabled(c))) return NULL;

    char hdrs[1200] = {0};
    if (needs_auth) {
        char *bearer = relay_media_client_bearer(c);
        snprintf(hdrs, sizeof(hdrs), "Authorization: Bearer %s\r\n", bearer ? bearer : "");
        free(bearer);
    }
    http_t *h = http_new(RELAY_REQUEST_TIMEOUT_S);
    if (!h) return NULL;
    http_resp_t *resp = http_get(h, url, hdrs);
    http_free(h);
    if (!resp || resp->status != 200 || !resp->body) {
        if (resp) http_resp_free(resp);
        return NULL;
    }
    size_t data_len = resp->body_len;
    if (data_len == 0 || data_len > RELAY_MEDIA_MAX_BYTES) {
        http_resp_free(resp);
        return NULL;
    }
    /* Extension: prefer suggested name suffix; fall back to .bin */
    const char *ext = ".bin";
    if (suggested_name && suggested_name[0]) {
        const char *dot = strrchr(suggested_name, '.');
        if (dot && dot[1]) ext = dot;
    }
    char tmpl[64];
    snprintf(tmpl, sizeof(tmpl), "/tmp/relay_media_XXXXXX");
    int fd = mkstemp(tmpl);
    if (fd < 0) { http_resp_free(resp); return NULL; }
    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s%s", tmpl, ext);
    if (rename(tmpl, final_path) != 0) {
        snprintf(final_path, sizeof(final_path), "%s", tmpl);
    }
    FILE *out = fopen(final_path, "wb");
    if (!out) { close(fd); unlink(tmpl); http_resp_free(resp); return NULL; }
    fwrite(resp->body, 1, data_len, out);
    fclose(out);
    close(fd);
    http_resp_free(resp);
    return strdup(final_path);
}
