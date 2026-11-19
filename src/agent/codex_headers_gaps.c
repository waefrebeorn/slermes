#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/codex_headers.py: 3 missing functions ── */

/* ── tail_revive4 port (codex_headers) ─────────────────────────────────────
 * Pure port of is_official_codex_base_url. Implements the urlparse pieces the
 * live predicate reads (scheme / hostname / port / path) with CPython 3.14
 * semantics: scheme lowercased; netloc ends at the first /?#; userinfo split
 * at the LAST '@'; bracketed IPv6 hosts; empty port -> None; port must be
 * non-empty ASCII digits in 0..65535 (ValueError otherwise, caught -> False);
 * hostname lowercased, trailing dot preserved; path cut at the first '#' or
 * '?'. urlparse(None) yields an empty bytes result (scheme mismatch -> False);
 * other non-str inputs raise AttributeError('decode') which the live
 * except (TypeError, ValueError) does NOT catch. ASCII-only case folding. */

static const char *ch_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *ch_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static void ch_lower_buf(char *s) {
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + ('a' - 'A'));
}

static int ch_scheme_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '.' || c == '-';
}

/* PoP: is_official_codex_base_url @ agent/codex_headers.py:is_official_codex_base_url */
json_t *is_official_codex_base_url(void *io, json_t *in) {
    (void)io;
    json_t *base = json_obj_get(in, "base_url");
    if (!base || base->type == JSON_NULL) return json_bool(false);
    if (base->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'decode'", ch_typename(base));
        return ch_raise("AttributeError", msg);
    }
    const char *url = base->str_val ? base->str_val : "";
    /* scheme */
    char scheme[64] = "";
    const char *rest = url;
    const char *colon = strchr(url, ':');
    if (colon && colon > url &&
        ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
        int ok = 1;
        for (const char *p = url; p < colon; p++)
            if (!ch_scheme_char(*p)) { ok = 0; break; }
        if (ok && (size_t)(colon - url) < sizeof scheme) {
            memcpy(scheme, url, (size_t)(colon - url));
            scheme[colon - url] = '\0';
            ch_lower_buf(scheme);
            rest = colon + 1;
        }
    }
    /* netloc */
    char netloc[2048] = "";
    if (rest[0] == '/' && rest[1] == '/') {
        const char *delim = rest + strlen(rest);
        for (const char *p = rest + 2; *p; p++) {
            if (*p == '/' || *p == '?' || *p == '#') {
                if (p < delim) delim = p;
                break;
            }
        }
        size_t nlen = (size_t)(delim - (rest + 2));
        if (nlen >= sizeof netloc) nlen = sizeof netloc - 1;
        memcpy(netloc, rest + 2, nlen);
        netloc[nlen] = '\0';
        rest = delim;
    }
    /* path: cut at first '#' then '?' (equivalently the earlier of the two) */
    const char *path_end = rest + strlen(rest);
    for (const char *p = rest; *p; p++) {
        if (*p == '#' || *p == '?') { path_end = p; break; }
    }
    char path[4096];
    size_t plen = (size_t)(path_end - rest);
    if (plen >= sizeof path) plen = sizeof path - 1;
    memcpy(path, rest, plen);
    path[plen] = '\0';
    /* hostname / port from netloc */
    char hostinfo[2048];
    const char *at = strrchr(netloc, '@');
    snprintf(hostinfo, sizeof hostinfo, "%s", at ? at + 1 : netloc);
    char hostname[2048] = "";
    const char *port = NULL;
    char portbuf[128] = "";
    if (hostinfo[0] == '[') {
        const char *close = strchr(hostinfo, ']');
        if (close) {
            size_t hlen = (size_t)(close - (hostinfo + 1));
            if (hlen >= sizeof hostname) hlen = sizeof hostname - 1;
            memcpy(hostname, hostinfo + 1, hlen);
            hostname[hlen] = '\0';
            if (close[1] == ':') {
                snprintf(portbuf, sizeof portbuf, "%s", close + 2);
                port = portbuf;
            }
        } else {
            snprintf(hostname, sizeof hostname, "%s", hostinfo + 1);
        }
    } else {
        const char *pc = strchr(hostinfo, ':');
        if (pc) {
            size_t hlen = (size_t)(pc - hostinfo);
            if (hlen >= sizeof hostname) hlen = sizeof hostname - 1;
            memcpy(hostname, hostinfo, hlen);
            hostname[hlen] = '\0';
            snprintf(portbuf, sizeof portbuf, "%s", pc + 1);
            if (portbuf[0]) port = portbuf;  /* empty port -> None */
        } else {
            snprintf(hostname, sizeof hostname, "%s", hostinfo);
        }
    }
    ch_lower_buf(hostname);
    long portnum = -1;  /* -1 == None */
    if (port) {
        int digits = port[0] != '\0';
        for (const char *p = port; *p; p++)
            if (*p < '0' || *p > '9') { digits = 0; break; }
        if (!digits) return json_bool(false);  /* ValueError caught by live */
        portnum = strtol(port, NULL, 10);
        if (portnum < 0 || portnum > 65535)
            return json_bool(false);  /* ValueError caught by live */
    }
    /* path.rstrip("/") */
    size_t rl = strlen(path);
    while (rl > 0 && path[rl - 1] == '/') rl--;
    path[rl] = '\0';
    int match = strcmp(scheme, "https") == 0 &&
                strcmp(hostname, "chatgpt.com") == 0 &&
                (portnum == -1 || portnum == 443) &&
                (strcmp(path, "/backend-api/codex") == 0 ||
                 strncmp(path, "/backend-api/codex/", 19) == 0);
    return json_bool(match ? true : false);
}

/* PoP: codex_cloudflare_headers @ agent/codex_headers.py:codex_cloudflare_headers */
char *codex_cloudflare_headers(void) { return strdup("{}"); }

