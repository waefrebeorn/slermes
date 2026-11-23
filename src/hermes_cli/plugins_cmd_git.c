/*
 * plugins_cmd_git.c — C home for upstream Hermes hermes_cli/plugins_cmd_git.py.
 *
 * PoP: w13_pluginscmd_56fc85_pinned_revision @ hermes_cli/plugins_cmd_git.py:pinned_revision
 * PoP: w13_pluginscmd_56fc85_pin_annotation @ hermes_cli/plugins_cmd_git.py:_pin_annotation
 * PoP: w13_pluginscmd_56fc85_normalize_exact_revision @ hermes_cli/plugins_cmd_git.py:_normalize_exact_revision
 * PoP: w13_pluginscmd_56fc85_scrub_git_url @ hermes_cli/plugins_cmd_git.py:_scrub_git_url
 * PoP: w13_pluginscmd_56fc85_canonical_source @ hermes_cli/plugins_cmd_git.py:_canonical_source
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginscmd_56fc85.h"

static char *dups(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static bool is_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static const json_t *pinned_rev(const json_t *in) {
    const char *name = json_get_str(in, "name", "");
    const json_t *metadata = json_obj_get(in, "metadata");
    const json_t *entry = (metadata && metadata->type == JSON_OBJECT) ? json_obj_get(metadata, name) : NULL;
    if (entry && entry->type == JSON_OBJECT && is_true(json_obj_get(entry, "pinned"))) {
        const json_t *rev = json_obj_get(entry, "revision");
        if (rev && rev->type == JSON_STRING) return rev;
    }
    return NULL;
}

json_t *w13_pluginscmd_56fc85_pinned_revision(const struct json_t *in) {
    const json_t *rev = pinned_rev(in);
    return rev ? json_string(rev->str_val) : json_null();
}

json_t *w13_pluginscmd_56fc85_pin_annotation(const struct json_t *in) {
    const json_t *rev = pinned_rev(in);
    if (rev && rev->str_val && rev->str_val[0]) {
        char buf[16];
        size_t n = strlen(rev->str_val) < 8 ? strlen(rev->str_val) : 8;
        memcpy(buf, rev->str_val, n);
        buf[n] = '\0';
        {
            char out[32];
            snprintf(out, sizeof(out), "git pinned@%s", buf);
            return json_string(out);
        }
    }
    return json_null();
}

json_t *w13_pluginscmd_56fc85_normalize_exact_revision(const struct json_t *in) {
    const json_t *ref = json_obj_get(in, "ref");
    json_t *out = json_object();
    if (ref && ref->type == JSON_STRING && strlen(ref->str_val) == 40) {
        char lower[41];
        size_t i;
        for (i = 0; i < 40; i++) {
            char c = ref->str_val[i];
            if (!isxdigit((unsigned char)c)) {
                json_set(out, "ok", json_bool(false));
                json_set(out, "error", json_string("--ref must be a full 40-character commit SHA."));
                return out;
            }
            lower[i] = (char)tolower((unsigned char)c);
        }
        lower[40] = '\0';
        json_set(out, "ok", json_bool(true));
        json_set(out, "value", json_string(lower));
    } else {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string("--ref must be a full 40-character commit SHA."));
    }
    return out;
}

/* urllib.parse.urlsplit-based scrub for http(s) URLs. */
static char *scrub_url(const char *url) {
    const char *p = url ? url : "";
    const char *sep = strstr(p, "://");
    char *scheme, *authority, *path;
    size_t slen, alen, plen, tail;
    const char *rest, *auth_start, *auth_end, *slash, *qmark, *hashmark, *at;
    char *host, *out;
    bool v6 = false;
    if (!sep) return dups(p);
    slen = (size_t)(sep - p);
    if (!((slen == 4 && strncasecmp(p, "http", 4) == 0) ||
          (slen == 5 && strncasecmp(p, "https", 5) == 0))) return dups(p);
    rest = sep + 3;
    /* authority ends at the first '/', '?' or '#'. */
    slash = strchr(rest, '/');
    qmark = strchr(rest, '?');
    hashmark = strchr(rest, '#');
    auth_end = rest + strlen(rest);
    if (slash && slash < auth_end) auth_end = slash;
    if (qmark && qmark < auth_end) auth_end = qmark;
    if (hashmark && hashmark < auth_end) auth_end = hashmark;
    alen = (size_t)(auth_end - rest);
    auth_start = rest;
    {
        size_t j;
        at = NULL;
        for (j = alen; j > 0; j--) {
            if (auth_start[j - 1] == '@') { at = auth_start + j - 1; break; }
        }
    }
    if (at) { auth_start = at + 1; alen = (size_t)(auth_end - auth_start); }
    /* path is from auth_end up to '?' or '#'. */
    {
        const char *pend = auth_end + strlen(auth_end);
        const char *qq = strpbrk(auth_end, "?#");
        if (qq && qq < pend) pend = qq;
        plen = (size_t)(pend - auth_end);
    }
    path = (char *)malloc(plen + 1);
    if (!path) return dups(p);
    if (plen) memcpy(path, auth_end, plen);
    path[plen] = '\0';
    /* host[:port] — bracketed IPv6 stays bracketed. */
    if (*auth_start == '[') {
        const char *rb = memchr(auth_start, ']', alen);
        if (rb) {
            size_t hlen = (size_t)(rb - auth_start) + 1;
            host = (char *)malloc(hlen + 1);
            if (!host) { free(path); return dups(p); }
            memcpy(host, auth_start, hlen); host[hlen] = '\0';
            v6 = true;
            (void)v6;
            tail = (size_t)((auth_start + alen) - (rb + 1));
        } else { free(path); return dups(p); }
    } else {
        const char *colon = memchr(auth_start, ':', alen);
        size_t hlen = colon ? (size_t)(colon - auth_start) : alen;
        host = (char *)malloc(hlen + 1);
        if (!host) { free(path); return dups(p); }
        memcpy(host, auth_start, hlen); host[hlen] = '\0';
        /* hostname is lowercased by urlsplit. */
        { size_t i; for (i = 0; i < hlen; i++) host[i] = (char)tolower((unsigned char)host[i]); }
        tail = colon ? (size_t)((auth_start + alen) - colon) : 0;
    }
    /* tail includes bracketed-IPv6 colon+port or ':<port>'; keep as-is. */
    {
        char *portpart = (char *)malloc(tail + 1);
        if (!portpart) { free(host); free(path); return dups(p); }
        if (tail) memcpy(portpart, auth_start + alen - tail, tail);
        portpart[tail] = '\0';
        out = (char *)malloc(slen + 3 + strlen(host) + strlen(portpart) + strlen(path) + 1);
        if (out) {
            char *w = out;
            size_t i;
            for (i = 0; i < slen; i++) *w++ = (char)tolower((unsigned char)p[i]);
            memcpy(w, "://", 3); w += 3;
            memcpy(w, host, strlen(host)); w += strlen(host);
            memcpy(w, portpart, strlen(portpart)); w += strlen(portpart);
            memcpy(w, path, strlen(path)); w += strlen(path);
            *w = '\0';
        }
        free(portpart);
    }
    free(host); free(path);
    return out ? out : dups(p);
}

json_t *w13_pluginscmd_56fc85_scrub_git_url(const struct json_t *in) {
    char *s = scrub_url(json_get_str(in, "git_url", ""));
    json_t *out = json_string(s);
    free(s);
    return out;
}

json_t *w13_pluginscmd_56fc85_canonical_source(const struct json_t *in) {
    char *s = scrub_url(json_get_str(in, "git_url", ""));
    const char *subdir = json_get_str(in, "subdir", "");
    json_t *out;
    if (subdir && subdir[0]) {
        size_t n = strlen(s) + strlen(subdir) + 2;
        char *buf = (char *)malloc(n);
        if (buf) { snprintf(buf, n, "%s#%s", s, subdir); free(s); s = buf; }
    }
    out = json_string(s);
    free(s);
    return out;
}
