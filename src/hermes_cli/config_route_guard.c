/*
 * config_route_guard.c - C home for upstream Hermes hermes_cli/config_route_guard.py.
 *
 * PoP: w17_configrout_51eafb_route_of @ hermes_cli/config_route_guard.py:_route_of
 * PoP: w17_configrout_51eafb_url @ hermes_cli/config_route_guard.py:_url
 * PoP: w17_configrout_51eafb_clear_aux_credentials @ hermes_cli/config_route_guard.py:clear_aux_credentials_on_route_change
 *
 * normalize_route_base_url is mirrored from hermes_cli/route_identity.py and
 * same_base_url_origin from utils/urls.py; credential clearing mirrors
 * hermes_cli.config.clear_model_endpoint_credentials.
 */
#include "noc_w17_configrout_51eafb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s) + 1;
    d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *strip_copy(const char *s) {
    const char *a = s ? s : "", *b;
    char *d;
    size_t n;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    d = (char *)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, a, n);
    d[n] = '\0';
    return d;
}

static char *strip_lower(const char *s) {
    char *d = strip_copy(s), *p;
    if (!d) return NULL;
    for (p = d; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z') *p = (char)(c + 32);
    }
    return d;
}

typedef struct {
    char scheme[64];
    char netloc[1024];
    char host[1024];
    char path[2048];
    char query[1024];
    int has_port;
    int port;
    int had_q;
} UrlParts;

/* Tolerate a bare host[:port][/path] like utils._parse_base_url("//"+raw). */
static void parse_url(const char *raw, UrlParts *u) {
    char *main, *q, *sep, *rest, *slash, *at, *hp, *colon, *closeb;
    char hostbuf[1024];
    size_t mainlen;
    const char *hash;
    memset(u, 0, sizeof(*u));
    hash = strchr(raw ? raw : "", '#');
    mainlen = hash ? (size_t)(hash - raw) : strlen(raw ? raw : "");
    main = (char *)malloc(mainlen + 1);
    if (!main) return;
    memcpy(main, raw ? raw : "", mainlen);
    main[mainlen] = '\0';
    q = strchr(main, '?');
    if (q) {
        *q = '\0';
        snprintf(u->query, sizeof(u->query), "%s", q + 1);
        u->had_q = 1;
    }
    sep = strstr(main, "://");
    if (sep) {
        size_t sl = (size_t)(sep - main);
        snprintf(u->scheme, sizeof(u->scheme), "%.*s", (int)sl, main);
        rest = sep + 3;
    } else {
        rest = main;
    }
    slash = strchr(rest, '/');
    if (slash) {
        snprintf(u->netloc, sizeof(u->netloc), "%.*s", (int)(slash - rest), rest);
        snprintf(u->path, sizeof(u->path), "%s", slash);
    } else {
        snprintf(u->netloc, sizeof(u->netloc), "%s", rest);
        u->path[0] = '\0';
    }
    at = strrchr(u->netloc, '@');
    hp = at ? at + 1 : u->netloc;
    if (*hp == '[') {
        closeb = strchr(hp, ']');
        if (closeb) snprintf(hostbuf, sizeof(hostbuf), "%.*s", (int)(closeb - hp - 1), hp + 1);
        else snprintf(hostbuf, sizeof(hostbuf), "%s", hp);
    } else {
        colon = strrchr(hp, ':');
        if (colon) {
            snprintf(hostbuf, sizeof(hostbuf), "%.*s", (int)(colon - hp), hp);
            if (colon[1]) { u->port = atoi(colon + 1); u->has_port = 1; }
        } else {
            snprintf(hostbuf, sizeof(hostbuf), "%s", hp);
        }
    }
    {
        size_t hl = strlen(hostbuf), i;
        while (hl > 0 && hostbuf[hl - 1] == '.') hostbuf[--hl] = '\0';
        for (i = 0; i < hl; ++i) {
            unsigned char c = (unsigned char)hostbuf[i];
            u->host[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
        }
        u->host[hl] = '\0';
    }
    {
        size_t sl = strlen(u->scheme), i;
        for (i = 0; i < sl; ++i) {
            unsigned char c = (unsigned char)u->scheme[i];
            u->scheme[i] = (char)(c >= 'A' && c <= 'Z' ? (c + 32) : c);
        }
    }
    free(main);
}

static int has_ctrl(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    for (; *p; ++p) if (*p <= 0x20) return 1;
    return 0;
}

/* Mirror of route_identity.normalize_route_base_url. Caller frees result. */
static char *normalize_route(const char *raw) {
    UrlParts u;
    char host[1200], tmp[1200], path[2048], out[4096];
    const char *at;
    size_t pl;
    if (!raw || !*raw) return dupstr("");
    if (has_ctrl(raw)) return dupstr(raw);
    parse_url(raw, &u);
    if (!u.scheme[0] || !u.host[0]) return dupstr(raw);
    snprintf(host, sizeof(host), "%s", u.host);
    if (strchr(host, '%')) {
        char *pct = strchr(host, '%');
        char addr[1200];
        snprintf(addr, sizeof(addr), "%s", pct + 1);
        *pct = '\0';
        snprintf(tmp, sizeof(tmp), "%s%%%s", host, addr);
        snprintf(host, sizeof(host), "%s", tmp);
    }
    at = strrchr(u.netloc, '@');
    if (u.netloc[0] == '[' || strchr(host, ':')) {
        snprintf(tmp, sizeof(tmp), "[%s]", host);
        snprintf(host, sizeof(host), "%s", tmp);
    }
    if (u.has_port && !((!strcmp(u.scheme, "http") && u.port == 80) ||
                        (!strcmp(u.scheme, "https") && u.port == 443))) {
        snprintf(tmp, sizeof(tmp), "%s:%d", host, u.port);
        snprintf(host, sizeof(host), "%s", tmp);
    }
    if (at) {
        size_t alen = (size_t)(at - u.netloc);
        snprintf(tmp, sizeof(tmp), "%.*s@%s", (int)alen, u.netloc, host);
        snprintf(host, sizeof(host), "%s", tmp);
    }
    snprintf(path, sizeof(path), "%s", u.path);
    pl = strlen(path);
    if (pl > 0 && path[pl - 1] == '/' && !u.had_q) path[pl - 1] = '\0';
    snprintf(out, sizeof(out), "%s://%s%s%s%s", u.scheme, host, path,
             u.query[0] ? "?" : "", u.query);
    if (u.had_q && !u.query[0]) strncat(out, "?", sizeof(out) - strlen(out) - 1);
    return dupstr(out);
}

static int origin_equal(const char *a, const char *b) {
    UrlParts x, y;
    int pa, pb;
    parse_url(a ? a : "", &x);
    parse_url(b ? b : "", &y);
    if (!x.host[0]) return 0;
    pa = x.has_port ? x.port : (!strcmp(x.scheme, "https") ? 443 : !strcmp(x.scheme, "http") ? 80 : 0);
    pb = y.has_port ? y.port : (!strcmp(y.scheme, "https") ? 443 : !strcmp(y.scheme, "http") ? 80 : 0);
    return !strcmp(x.host, y.host) && !strcmp(x.scheme, y.scheme) && pa == pb;
}

/* _route_of: (provider stripped/lowered with auto->"", normalized stripped url) */
static void route_of(const char *provider, const char *base_url, char **pp, char **uu) {
    char *p = strip_lower(provider);
    char *b = strip_copy(base_url);
    char *n = normalize_route(b ? b : "");
    if (p && !strcmp(p, "auto")) { free(p); p = dupstr(""); }
    *pp = p ? p : dupstr("");
    *uu = n ? n : dupstr("");
    free(b);
}

json_t *w17_configrout_51eafb_route_of(const char *provider, const char *base_url) {
    char *p, *u;
    json_t *arr = json_array();
    route_of(provider, base_url, &p, &u);
    json_append(arr, json_string(p ? p : ""));
    json_append(arr, json_string(u ? u : ""));
    free(p);
    free(u);
    return arr;
}

/* _url: str(value or "").strip().rstrip("/") */
json_t *w17_configrout_51eafb_url(const char *value) {
    char *s = strip_copy(value);
    size_t n;
    if (!s) return json_string("");
    n = strlen(s);
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
    return json_string(s);
}

json_t *w17_configrout_51eafb_clear_aux_credentials(const json_t *entry,
                                                    const char *provider,
                                                    const char *base_url) {
    json_t *ecopy = (entry && entry->type == JSON_OBJECT) ? json_copy(entry) : json_object();
    json_t *res = json_object();
    const char *oldp, *oldb;
    char *op_s, *ou_s, *np_s, *nu_s;
    int same, custom_same, changed = 0;
    oldp = json_get_str(ecopy, "provider", "");
    oldb = json_get_str(ecopy, "base_url", "");
    route_of(oldp, oldb, &op_s, &ou_s);
    route_of(provider, base_url, &np_s, &nu_s);
    same = !strcmp(op_s, np_s) && !strcmp(ou_s, nu_s);
    custom_same = !strcmp(np_s, "custom") && nu_s[0] && ou_s[0] && origin_equal(ou_s, nu_s);
    if (!same && !custom_same) {
        const char *fields[] = {"api_key", "api", "key_env", "api_key_env", "api_mode", NULL};
        int i;
        for (i = 0; fields[i]; ++i) json_obj_del(ecopy, fields[i]);
        changed = 1;
    }
    free(op_s); free(ou_s); free(np_s); free(nu_s);
    json_set(res, "changed", json_bool(changed ? true : false));
    json_set(res, "entry", ecopy);
    return res;
}
