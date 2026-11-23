/*
 * methods_browser.c — faithful C11 port of the deterministic CDP URL
 * classification in tui_gateway/methods_browser.py.
 * PoP: w15_methodsbro_2acb98_is_default_local_cdp @ tui_gateway/methods_browser.py:_is_default_local_cdp
 * PoP: w15_methodsbro_2acb98_cdp_schemes          @ tui_gateway/methods_browser.py:_CDP_SCHEMES
 *
 * The live connect/disconnect handlers probe the network and drive a browser;
 * only the no-I/O default-local classifier and the scheme literal are ported.
 */
#include "noc_w15_methodsbro_2acb98.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char scheme[64];
    char host[512];
    int port;
    int has_port;
    int port_error;
    char path[512];
} cdp_url;

static void lower_ascii(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

/* Minimal urlparse for the http/ws URLs this classifier accepts. */
static void parse_url(const char *url, cdp_url *u) {
    memset(u, 0, sizeof(*u));
    if (!url) return;
    const char *sep = strstr(url, "://");
    const char *rest;
    if (sep) {
        size_t n = (size_t)(sep - url);
        if (n >= sizeof(u->scheme)) n = sizeof(u->scheme) - 1;
        memcpy(u->scheme, url, n);
        u->scheme[n] = '\0';
        lower_ascii(u->scheme);
        rest = sep + 3;
    } else {
        rest = url;
    }
    const char *end = rest;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t alen = (size_t)(end - rest);
    char auth[1024];
    if (alen >= sizeof(auth)) alen = sizeof(auth) - 1;
    memcpy(auth, rest, alen);
    auth[alen] = '\0';

    const char *hostpart = auth;
    const char *at = strrchr(auth, '@');
    if (at) hostpart = at + 1;
    const char *portpart = NULL;
    char hostbuf[1024];
    if (*hostpart == '[') {
        const char *close = strchr(hostpart, ']');
        if (close) {
            size_t hn = (size_t)(close - hostpart - 1);
            if (hn >= sizeof(hostbuf)) hn = sizeof(hostbuf) - 1;
            memcpy(hostbuf, hostpart + 1, hn);
            hostbuf[hn] = '\0';
            if (close[1] == ':') portpart = close + 2;
        } else {
            hostbuf[0] = '\0';
        }
    } else {
        const char *colon = strchr(hostpart, ':');
        if (colon) {
            size_t hn = (size_t)(colon - hostpart);
            if (hn >= sizeof(hostbuf)) hn = sizeof(hostbuf) - 1;
            memcpy(hostbuf, hostpart, hn);
            hostbuf[hn] = '\0';
            portpart = colon + 1;
        } else {
            size_t hn = strlen(hostpart);
            if (hn >= sizeof(hostbuf)) hn = sizeof(hostbuf) - 1;
            memcpy(hostbuf, hostpart, hn);
            hostbuf[hn] = '\0';
        }
    }
    size_t hn = strlen(hostbuf);
    if (hn >= sizeof(u->host)) hn = sizeof(u->host) - 1;
    memcpy(u->host, hostbuf, hn);
    u->host[hn] = '\0';
    lower_ascii(u->host);

    if (portpart && *portpart) {
        char *p;
        long v = strtol(portpart, &p, 10);
        while (*p == ' ') p++;
        if (*p || v < 0 || v > 65535) u->port_error = 1;
        else { u->port = (int)v; u->has_port = 1; }
    }

    if (*end) {
        const char *q = end;
        while (*q && *q != '?' && *q != '#') q++;
        size_t pn = (size_t)(q - end);
        if (pn >= sizeof(u->path)) pn = sizeof(u->path) - 1;
        memcpy(u->path, end, pn);
        u->path[pn] = '\0';
    }
}

int w15_methodsbro_2acb98_is_default_local_cdp(const char *url) {
    cdp_url u;
    parse_url(url, &u);
    if (u.port_error) return 0;
    if (strcmp(u.scheme, "http") && strcmp(u.scheme, "ws")) return 0;
    if (strcmp(u.host, "127.0.0.1") && strcmp(u.host, "localhost")) return 0;
    int port = u.has_port ? u.port : 80;
    if (port != 9222) return 0;
    if (strcmp(u.path, "") && strcmp(u.path, "/") &&
        strcmp(u.path, "/json") && strcmp(u.path, "/json/version")) return 0;
    return 1;
}

json_t *w15_methodsbro_2acb98_cdp_schemes(void) {
    json_t *out = json_array();
    json_append(out, json_string("http"));
    json_append(out, json_string("https"));
    json_append(out, json_string("ws"));
    json_append(out, json_string("wss"));
    return out;
}
