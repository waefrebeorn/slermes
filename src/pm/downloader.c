/*
 * downloader.c — faithful C11 port of the pure helpers in pm/downloader.py.
 *
 * PoP: w5_downloader_69e11a_pinned @ pm/downloader.py:_pinned
 * PoP: w5_downloader_69e11a_host @ pm/downloader.py:_host
 * PoP: w5_downloader_69e11a_loopback @ pm/downloader.py:_loopback
 * PoP: w5_downloader_69e11a_coalesce @ pm/downloader.py:_coalesce
 * PoP: w5_downloader_69e11a_missing @ pm/downloader.py:_missing
 * PoP: w5_downloader_69e11a_source_plain_http @ pm/downloader.py:Source.plain_http
 * PoP: w5_downloader_69e11a_source_byte_limit @ pm/downloader.py:Source.byte_limit
 * PoP: w5_downloader_69e11a_validate_range @ pm/downloader.py:_validate_range
 * PoP: w5_downloader_69e11a_strong_etag @ pm/downloader.py:_strong_etag
 */
#include "noc_w5_downloader_69e11a.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <stdint.h>

typedef struct { int64_t a, b; } w5d_run;
typedef struct { w5d_run *v; size_t n, cap; } w5d_runs;

static void w5d_run_push(w5d_runs *s, w5d_run r) {
    if (s->n >= s->cap) {
        size_t need = s->cap ? s->cap * 2 : 8;
        w5d_run *tmp = (w5d_run *)realloc(s->v, need * sizeof(w5d_run));
        if (!tmp) return;
        s->v = tmp; s->cap = need;
    }
    s->v[s->n++] = r;
}
static char *w5d_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *w5d_lower(const char *s, size_t n) {
    char *p = w5d_dupn(s, n);
    if (p) for (size_t i = 0; i < n; i++) p[i] = (char)tolower((unsigned char)p[i]);
    return p;
}
static int64_t w5d_int(const struct json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_NUMBER) return (int64_t)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}
static struct json_t *w5d_run_json(w5d_run r) {
    struct json_t *a = json_array();
    json_append(a, json_number((double)r.a));
    json_append(a, json_number((double)r.b));
    return a;
}

struct json_t *w5_downloader_69e11a_pinned(const char *sha256) {
    int ok = sha256 && strlen(sha256) == 64;
    if (ok) for (const char *p = sha256; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) { ok = 0; break; }
    return json_bool(ok ? true : false);
}

typedef struct { char *scheme, *netloc, *host; int bad; } w5d_url;

static void w5d_url_free(w5d_url *u) {
    free(u->scheme); free(u->netloc); free(u->host);
    u->scheme = u->netloc = u->host = NULL; u->bad = 0;
}

static w5d_url w5d_urlsplit(const char *url) {
    w5d_url u = {0};
    if (!url) url = "";
    size_t len = strlen(url);
    size_t rest = 0;
    size_t i = 0;
    while (i < len && (isalnum((unsigned char)url[i]) || url[i] == '+' || url[i] == '-' || url[i] == '.')) i++;
    if (i > 0 && isalpha((unsigned char)url[0]) && url[i] == ':') {
        u.scheme = w5d_lower(url, i);
        rest = i + 1;
    } else {
        u.scheme = w5d_dupn("", 0);
    }
    size_t start = 0;
    if (rest > 0 && rest + 1 < len && url[rest] == '/' && url[rest + 1] == '/') start = rest + 2;
    else if (rest == 0 && len >= 2 && url[0] == '/' && url[1] == '/') start = 2;
    if (start) {
        size_t end = start;
        while (end < len && url[end] != '/' && url[end] != '?' && url[end] != '#') end++;
        u.netloc = w5d_dupn(url + start, end - start);
    } else {
        u.netloc = w5d_dupn("", 0);
    }
    const char *userfree = u.netloc;
    const char *at = strrchr(u.netloc, '@');
    if (at) userfree = at + 1;
    if (strchr(u.netloc, '[') && !strchr(u.netloc, ']')) { u.bad = 1; return u; }
    if (strchr(u.netloc, ']') && !strchr(u.netloc, '[')) { u.bad = 1; return u; }
    if (*userfree == '[') {
        const char *close = strchr(userfree, ']');
        if (!close) { u.bad = 1; return u; }
        u.host = w5d_lower(userfree + 1, (size_t)(close - userfree - 1));
    } else {
        const char *colon = strchr(userfree, ':');
        u.host = w5d_lower(userfree, colon ? (size_t)(colon - userfree) : strlen(userfree));
    }
    if (u.host && !*u.host) { free(u.host); u.host = NULL; }
    return u;
}

struct json_t *w5_downloader_69e11a_host(const char *url) {
    w5d_url u = w5d_urlsplit(url);
    struct json_t *r = (u.bad || !u.host) ? json_null() : json_string(u.host);
    w5d_url_free(&u);
    return r;
}

struct json_t *w5_downloader_69e11a_loopback(const char *url) {
    w5d_url u = w5d_urlsplit(url);
    int ok = 0;
    if (!u.bad && u.scheme && !strcmp(u.scheme, "http") && u.netloc &&
        !strchr(u.netloc, '@') && u.host &&
        (!strcmp(u.host, "127.0.0.1") || !strcmp(u.host, "localhost") || !strcmp(u.host, "::1")))
        ok = 1;
    w5d_url_free(&u);
    return json_bool(ok ? true : false);
}

static w5d_runs w5d_collect_runs(const struct json_t *ranges) {
    w5d_runs out = {0};
    if (!ranges || ranges->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(ranges); i++) {
        const struct json_t *p = json_get(ranges, i);
        if (!p || p->type != JSON_ARRAY || json_len(p) < 2) continue;
        w5d_run r = { w5d_int(json_get(p, 0)), w5d_int(json_get(p, 1)) };
        if (r.b > r.a) w5d_run_push(&out, r);
    }
    for (size_t i = 1; i < out.n; i++) {
        size_t j = i;
        while (j > 0 && (out.v[j-1].a > out.v[j].a || (out.v[j-1].a == out.v[j].a && out.v[j-1].b > out.v[j].b))) {
            w5d_run t = out.v[j-1]; out.v[j-1] = out.v[j]; out.v[j] = t; j--;
        }
    }
    return out;
}

static w5d_runs w5d_coalesce_runs(const struct json_t *ranges) {
    w5d_runs sorted = w5d_collect_runs(ranges);
    w5d_runs out = {0};
    if (!sorted.n) return out;
    w5d_run cur = sorted.v[0];
    for (size_t i = 1; i < sorted.n; i++) {
        w5d_run x = sorted.v[i];
        if (x.a <= cur.b) { if (x.b > cur.b) cur.b = x.b; }
        else { w5d_run_push(&out, cur); cur = x; }
    }
    w5d_run_push(&out, cur);
    free(sorted.v);
    return out;
}

struct json_t *w5_downloader_69e11a_coalesce(const struct json_t *ranges) {
    w5d_runs runs = w5d_coalesce_runs(ranges);
    struct json_t *out = json_array();
    for (size_t i = 0; i < runs.n; i++) json_append(out, w5d_run_json(runs.v[i]));
    free(runs.v);
    return out;
}

struct json_t *w5_downloader_69e11a_missing(const struct json_t *total,
                                            const struct json_t *covered) {
    int64_t limit = w5d_int(total);
    w5d_runs runs = w5d_coalesce_runs(covered);
    struct json_t *out = json_array();
    int64_t cursor = 0;
    for (size_t i = 0; i < runs.n; i++) {
        if (runs.v[i].a > cursor) {
            w5d_run gap = { cursor, runs.v[i].a };
            json_append(out, w5d_run_json(gap));
        }
        if (runs.v[i].b > cursor) cursor = runs.v[i].b;
    }
    if (cursor < limit) {
        w5d_run gap = { cursor, limit };
        json_append(out, w5d_run_json(gap));
    }
    free(runs.v);
    return out;
}

static int w5d_plain_http(const char *url, const char *sha256, const char *http_mirror) {
    if (!http_mirror || !*http_mirror || !url || !sha256) return 0;
    if (strncmp(url, http_mirror, strlen(http_mirror))) return 0;
    struct json_t *p = w5_downloader_69e11a_pinned(sha256);
    int ok = p->bool_val ? 1 : 0;
    json_free(p);
    return ok;
}

struct json_t *w5_downloader_69e11a_source_plain_http(const char *url, const char *sha256,
                                                      const char *http_mirror) {
    return json_bool(w5d_plain_http(url, sha256, http_mirror) ? true : false);
}

struct json_t *w5_downloader_69e11a_source_byte_limit(const char *url, const char *sha256,
                                                      const char *http_mirror) {
    if (!w5d_plain_http(url, sha256, http_mirror)) return json_null();
    return json_number((double)(256 << 20));
}

static struct json_t *w5d_error(const char *message) {
    struct json_t *o = json_object();
    json_set(o, "__error__", json_string(message));
    return o;
}

static int w5d_parse_u64(const char **p, int64_t *out) {
    const char *s = *p;
    if (!isdigit((unsigned char)*s)) return 0;
    const char *e = s;
    while (isdigit((unsigned char)*e)) e++;
    char *num = w5d_dupn(s, (size_t)(e - s));
    *out = strtoll(num, NULL, 10);
    free(num);
    *p = e;
    return 1;
}

struct json_t *w5_downloader_69e11a_validate_range(const struct json_t *status,
                        const char *content_range, const struct json_t *content_length,
                        const char *content_encoding, const struct json_t *start,
                        const struct json_t *end, const struct json_t *total) {
    const char *cr = content_range ? content_range : "";
    int64_t first = 0, last = 0, size = 0;
    int matched = 0;
    if (!strncmp(cr, "bytes ", 6)) {
        const char *p = cr + 6;
        if (w5d_parse_u64(&p, &first) && *p == '-') {
            p++;
            if (w5d_parse_u64(&p, &last) && *p == '/') {
                p++;
                if (w5d_parse_u64(&p, &size) && *p == '\0') matched = 1;
            }
        }
    }
    int64_t st = w5d_int(start), en = w5d_int(end);
    if (w5d_int(status) != 206 || !matched)
        return w5d_error("server did not honor the requested byte range");
    int has_total = total && total->type != JSON_NULL;
    if (first != st || last != en - 1 || size < en || (has_total && size != w5d_int(total)))
        return w5d_error("server returned a different byte range or representation size");
    if (content_length && content_length->type == JSON_STRING) {
        const char *s = content_length->str_val;
        int decimal = *s != '\0';
        for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) { decimal = 0; break; }
        if (!decimal || strtoll(s, NULL, 10) != en - st)
            return w5d_error("range Content-Length does not match its bounds");
    }
    const char *enc = content_encoding ? content_encoding : "identity";
    if (strcasecmp(enc, "identity"))
        return w5d_error("encoded response cannot be written into an identity byte range");
    return json_number((double)size);
}

struct json_t *w5_downloader_69e11a_strong_etag(const char *etag) {
    if (!etag) etag = "";
    size_t n = strlen(etag);
    int ok = n > 0 && etag[0] == '"' && etag[n - 1] == '"';
    return json_string(ok ? etag : "");
}

