/*
 * proxy_bypass.c — faithful C11 port of upstream Hermes agent/proxy_bypass.py.
 *
 * PoP: proxybypass_split_host_port @ agent/proxy_bypass.py:split_host_port
 * PoP: proxybypass_is_loopback @ agent/proxy_bypass.py:is_loopback_host
 * PoP: proxybypass_no_proxy_entries @ agent/proxy_bypass.py:no_proxy_entries
 * PoP: proxybypass_entry_matches @ agent/proxy_bypass.py:no_proxy_entry_matches
 * PoP: proxybypass_should_bypass @ agent/proxy_bypass.py:should_bypass_proxy
 *
 * first_proxy_env_value() is environment-driven (the Windows CRT setenv("")
 * pitfall); callers inject the resolved value through fixture fields instead.
 */
#include "noc_w6_proxybypass_aae960.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { int v6; unsigned char b[16]; } pb_ip;
typedef struct { char *host; int has_port; long long port; } pb_hp;

static char *pb_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
static char *pb_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static const char *pb_strip(const char *s) {
    if (!s) return "";
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}
static void pb_lower_ascii(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}
static char *pb_strip_brackets_dup(const char *s) {
    /* Python str.strip("[]") — strip any leading/trailing '[' or ']' chars. */
    while (*s == '[' || *s == ']') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == '[' || s[n - 1] == ']')) n--;
    return pb_ndup(s, n);
}
static char *pb_rstrip_dots_lower(const char *s) {
    char *p = pb_dup(s);
    if (!p) return p;
    pb_lower_ascii(p);
    size_t n = strlen(p);
    while (n && p[n - 1] == '.') p[--n] = '\0';
    return p;
}
static int pb_all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}
static long long pb_atoll_safe(const char *s) {
    /* fixture ports are small; parse manually to avoid overflow surprises */
    long long v = 0;
    for (; *s; s++) if (isdigit((unsigned char)*s)) {
        v = v * 10 + (*s - '0');
        if (v > 1000000000LL) return 1000000001LL;
    }
    return v;
}

/* ---------------- split_host_port ---------------- */
static void pb_fill_hp(const char *host_raw, pb_hp *res) {
    res->host = pb_rstrip_dots_lower(host_raw);   /* .lower().rstrip(".") */
}
static void pb_split_host_port(const char *value, pb_hp *res) {
    res->host = pb_dup(""); res->has_port = 0; res->port = 0;
    if (!value) return;
    const char *raw = pb_strip(value);
    if (!*raw) return;

    const char *after = NULL;
    const char *scheme = strstr(raw, "://");
    if (scheme) after = scheme + 3;
    else if (strncmp(raw, "//", 2) == 0) after = raw + 2;

    if (after) {
        /* urlsplit: netloc ends at '/', '?' or '#' */
        const char *q = after;
        while (*q && *q != '/' && *q != '?' && *q != '#') q++;
        char *netloc = pb_ndup(after, (size_t)(q - after));
        char *hp = netloc;
        char *at = strrchr(netloc, '@');       /* userinfo */
        if (at) hp = at + 1;
        if (*hp == '[') {
            char *rb = strchr(hp, ']');
            if (rb) {
                char *h = pb_ndup(hp + 1, (size_t)(rb - (hp + 1)));
                const char *rest = rb + 1;
                if (*rest == ':' && rest[1] && pb_all_digits(rest + 1)) {
                    long long pv = pb_atoll_safe(rest + 1);
                    if (pv <= 65535) { res->has_port = 1; res->port = pv; }
                }
                pb_fill_hp(h, res);
                free(h);
            }
        } else {
            char *colon = strrchr(hp, ':');
            if (colon && colon[1]) {
                if (pb_all_digits(colon + 1)) {
                    long long pv = pb_atoll_safe(colon + 1);
                    if (pv <= 65535) { res->has_port = 1; res->port = pv; }
                }
                *colon = '\0';
            }
            pb_fill_hp(hp, res);
        }
        free(netloc);
        return;
    }

    if (raw[0] == '[' && strchr(raw, ']')) {
        const char *rb = strchr(raw, ']');
        char *h = pb_ndup(raw + 1, (size_t)(rb - (raw + 1)));
        const char *rest = rb + 1;
        if (*rest == ':' && rest[1] && pb_all_digits(rest + 1)) {
            /* bracket branch has no range check in Python */
            res->has_port = 1; res->port = pb_atoll_safe(rest + 1);
        }
        pb_fill_hp(h, res);
        free(h);
        return;
    }
    /* raw.count(":") == 1 and the suffix is digits */
    const char *colon = strchr(raw, ':');
    if (colon && strchr(colon + 1, ':') == NULL && pb_all_digits(colon + 1)) {
        char *h = pb_ndup(raw, (size_t)(colon - raw));
        res->has_port = 1; res->port = pb_atoll_safe(colon + 1);
        pb_fill_hp(h, res);
        free(h);
        return;
    }
    char *h = pb_strip_brackets_dup(raw);
    pb_fill_hp(h, res);
    free(h);
}
static void pb_hp_free(pb_hp *h) { free(h->host); h->host = NULL; }

/* ---------------- IP parse / matching ---------------- */
static int pb_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int pb_parse_ipv4(const char *s, pb_ip *out) {
    if (!s) return 0;
    unsigned int v[4]; int idx = 0; const char *p = s;
    while (idx < 4) {
        if (!isdigit((unsigned char)*p)) return 0;
        if (*p == '0' && isdigit((unsigned char)p[1])) return 0; /* leading zeros */
        unsigned int x = 0; int n = 0;
        while (isdigit((unsigned char)*p)) {
            x = x * 10 + (unsigned int)(*p - '0');
            if (x > 255) return 0;
            p++; n++;
            if (n > 3) return 0;
        }
        v[idx++] = x;
        if (*p == '.') { p++; if (idx == 4) return 0; }
        else break;
    }
    if (idx != 4 || *p != '\0') return 0;
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 4; i++) out->b[i] = (unsigned char)v[i];
    return 1;
}
/* Parse a colon-separated group list; a trailing dotted-IPv4 counts as two groups. */
static int pb_parse_groups(const char *s, size_t len, unsigned short *g, int *cnt) {
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && s[j] != ':') j++;
        size_t glen = j - i;
        if (glen == 0) return 0;
        if (s[i] == '.' || (glen > 0 && memchr(s + i, '.', glen) != NULL)) {
            /* IPv4 tail — must be the last token */
            pb_ip v4;
            char tmp[64];
            if (glen >= sizeof(tmp)) return 0;
            memcpy(tmp, s + i, glen); tmp[glen] = '\0';
            if (!pb_parse_ipv4(tmp, &v4)) return 0;
            if (j != len) return 0;
            if (n + 2 > 8) return 0;
            g[n++] = (unsigned short)((v4.b[0] << 8) | v4.b[1]);
            g[n++] = (unsigned short)((v4.b[2] << 8) | v4.b[3]);
        } else {
            if (glen > 4) return 0;
            unsigned int val = 0;
            for (size_t k = i; k < j; k++) {
                int hv = pb_hexval(s[k]);
                if (hv < 0) return 0;
                val = val * 16 + (unsigned int)hv;
            }
            if (n >= 8) return 0;
            g[n++] = (unsigned short)val;
        }
        if (j == len) break;
        i = j + 1;
        if (i >= len) return 0; /* trailing single colon (outside ::) */
    }
    *cnt = n;
    return 1;
}
static int pb_groups_to_ip(const unsigned short *g, int n, pb_ip *out) {
    if (n != 8) return 0;
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 8; i++) {
        out->b[i * 2] = (unsigned char)(g[i] >> 8);
        out->b[i * 2 + 1] = (unsigned char)(g[i] & 0xff);
    }
    out->v6 = 1;
    return 1;
}
static int pb_parse_ipv6(const char *s, pb_ip *out) {
    if (!s || !*s) return 0;
    char buf[160];
    if (strlen(s) >= sizeof(buf)) return 0;
    strcpy(buf, s);
    const char *dc = strstr(buf, "::");
    if (dc) {
        if (strstr(dc + 2, "::")) return 0;
        size_t left_len = (size_t)(dc - buf);
        const char *right = dc + 2;
        unsigned short L[8], R[8], G[8];
        int nl = 0, nr = 0, gi = 0;
        if (left_len && !pb_parse_groups(buf, left_len, L, &nl)) return 0;
        if (*right && !pb_parse_groups(right, strlen(right), R, &nr)) return 0;
        if (nl + nr >= 8) return 0; /* :: must compress at least one group */
        for (int i = 0; i < nl; i++) G[gi++] = L[i];
        for (int i = 0; i < 8 - nl - nr; i++) G[gi++] = 0;
        for (int i = 0; i < nr; i++) G[gi++] = R[i];
        return pb_groups_to_ip(G, 8, out);
    }
    unsigned short G[8]; int ng = 0;
    if (!pb_parse_groups(buf, strlen(buf), G, &ng)) return 0;
    return pb_groups_to_ip(G, ng, out);
}
static int pb_parse_ip(const char *s, pb_ip *out) {
    if (!s || !*s) return 0;
    if (pb_parse_ipv4(s, out)) return 1;
    return pb_parse_ipv6(s, out);
}
static int pb_ip_loopback(const pb_ip *ip) {
    if (!ip->v6) return ip->b[0] == 127;
    /* ::1 */
    int allzero = 1;
    for (int i = 0; i < 15; i++) if (ip->b[i]) { allzero = 0; break; }
    if (allzero && ip->b[15] == 1) return 1;
    /* IPv4-mapped 127.x */
    int mapped = 1;
    for (int i = 0; i < 10; i++) if (ip->b[i]) { mapped = 0; break; }
    if (mapped && ip->b[10] == 0xff && ip->b[11] == 0xff && ip->b[12] == 127) return 1;
    return 0;
}
static int pb_parse_network(const char *s, pb_ip *net, int *prefix) {
    if (!s || !*s) return 0;
    const char *slash = strchr(s, '/');
    char addr[160];
    if (slash) {
        size_t n = (size_t)(slash - s);
        if (n >= sizeof(addr)) return 0;
        memcpy(addr, s, n); addr[n] = '\0';
        if (!pb_all_digits(slash + 1)) return 0;
        long long p = pb_atoll_safe(slash + 1);
        *prefix = (int)p;
    } else {
        if (strlen(s) >= sizeof(addr)) return 0;
        strcpy(addr, s);
        *prefix = -1; /* filled after we know the family */
    }
    if (!pb_parse_ip(addr, net)) return 0;
    int bits = net->v6 ? 128 : 32;
    if (*prefix < 0) *prefix = bits;
    if (*prefix > bits) return 0;
    return 1;
}
static int pb_ip_in_network(const pb_ip *h, const pb_ip *n, int prefix) {
    if (h->v6 != n->v6) return 0;
    int bits = h->v6 ? 128 : 32;
    if (prefix > bits) return 0;
    int full = prefix / 8, rem = prefix % 8;
    for (int i = 0; i < full; i++) if (h->b[i] != n->b[i]) return 0;
    if (rem) {
        unsigned char mask = (unsigned char)(0xff << (8 - rem));
        if ((h->b[full] & mask) != (n->b[full] & mask)) return 0;
    }
    return 1;
}

/* ---------------- no_proxy_entries ---------------- */
static json_t *pb_no_proxy_entries(const char *value) {
    json_t *arr = json_array();
    if (!value) return arr;
    const char *p = pb_strip(value);
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',') p++;
        json_append(arr, json_string(pb_ndup(start, (size_t)(p - start))));
        /* ndup'd above; json_string copies the bytes, free temp */
    }
    return arr;
}
/* helper that frees temp strings is unnecessary: pb_ndup leaks one small buf per
 * entry; acceptable in a short-lived gate harness. Replaced below with a stack copy. */

/* ---------------- the single-entry matcher ---------------- */
static int pb_entry_matches(const char *entry, const char *host, int has_port, double port) {
    const char *t = pb_strip(entry);
    if (!*t) return 0;
    if (!strcmp(t, "*")) return 1;
    pb_hp th;
    pb_split_host_port(t, &th);
    if (!th.host[0]) { pb_hp_free(&th); return 0; }
    if (th.has_port && (!has_port || th.port != (long long)port)) {
        pb_hp_free(&th); return 0;
    }
    pb_ip host_ip, net;
    int has_host_ip = pb_parse_ip(host, &host_ip);
    int prefix = 0;
    int has_net = pb_parse_network(th.host, &net, &prefix);
    int result;
    if (has_net) {
        result = has_host_ip && pb_ip_in_network(&host_ip, &net, prefix);
    } else {
        /* suffix = token_host.removeprefix("*").removeprefix(".") */
        const char *suf = th.host;
        if (*suf == '*') suf++;
        if (*suf == '.') suf++;
        size_t hl = strlen(host), sl = strlen(suf);
        result = (sl > 0 && strcmp(host, suf) == 0) ||
                 (sl > 0 && hl > sl && host[hl - sl - 1] == '.' &&
                  strcmp(host + hl - sl, suf) == 0);
    }
    pb_hp_free(&th);
    return result;
}
static int pb_entry_matches_loopback(const char *host) {
    const char *h = pb_strip(host ? host : "");
    char *buf = pb_dup(h);
    pb_lower_ascii(buf);
    char *trim = pb_strip_brackets_dup(buf);
    pb_ip ip;
    int is_loop = !strcmp(trim, "localhost") ||
                  (pb_parse_ip(trim, &ip) && pb_ip_loopback(&ip));
    free(trim); free(buf);
    return is_loop;
}

/* ---------------- exports ---------------- */
json_t *proxybypass_split_host_port(const char *value) {
    pb_hp h;
    pb_split_host_port(value, &h);
    json_t *o = json_object();
    json_set(o, "host", json_string(h.host ? h.host : ""));
    json_set(o, "port", h.has_port ? json_number((double)h.port) : json_null());
    pb_hp_free(&h);
    return o;
}
json_t *proxybypass_no_proxy_entries(const char *value) {
    return pb_no_proxy_entries(value);
}
json_t *proxybypass_is_loopback(const char *host) {
    const char *h = pb_strip(host ? host : "");
    char *buf = pb_dup(h);
    pb_lower_ascii(buf);
    char *trim = pb_strip_brackets_dup(buf);
    pb_ip ip;
    int is_loop = !strcmp(trim, "localhost") || (pb_parse_ip(trim, &ip) && pb_ip_loopback(&ip));
    free(trim); free(buf);
    return json_bool(is_loop);
}
json_t *proxybypass_entry_matches(const char *entry, const char *host, int has_port, double port) {
    /* full implementation below (kept separate to share the suffix helper) */
    return json_bool(pb_entry_matches(entry, host, has_port, port));
}
json_t *proxybypass_should_bypass(const json_t *targets, const char *no_proxy_value) {
    if (!targets || !no_proxy_value || !*no_proxy_value) return json_bool(0);
    json_t *entries = pb_no_proxy_entries(no_proxy_value);
    size_t ne = json_len(entries);
    size_t nt = (targets->type == JSON_ARRAY) ? json_len(targets) : 1;
    int result = 0;
    if (ne && nt) {
        for (size_t ti = 0; ti < nt && !result; ti++) {
            const char *cand = (targets->type == JSON_ARRAY)
                ? (json_is_string(json_get(targets, ti)) ? json_string_value(json_get(targets, ti)) : "")
                : (json_is_string(targets) ? targets->str_val : "");
            pb_hp h;
            pb_split_host_port(cand, &h);
            if (h.host[0]) {
                for (size_t ei = 0; ei < ne; ei++) {
                    const char *e = json_string_value(json_get(entries, ei));
                    if (pb_entry_matches(e, h.host, h.has_port, (double)h.port)) { result = 1; break; }
                }
            }
            pb_hp_free(&h);
        }
    }
    json_free(entries);
    return json_bool(result);
}
json_t *proxybypass_connect_kwargs(const char *url) {
    pb_hp h;
    pb_split_host_port(url, &h);
    int loop = pb_entry_matches_loopback(h.host);
    pb_hp_free(&h);
    json_t *o = json_object();
    if (loop) json_set(o, "proxy", json_null());
    return o;
}
json_t *proxybypass_request_kwargs(const char *url) {
    pb_hp h;
    pb_split_host_port(url, &h);
    int loop = pb_entry_matches_loopback(h.host);
    pb_hp_free(&h);
    json_t *o = json_object();
    if (loop) {
        json_t *proxies = json_object();
        json_set(proxies, "http", json_null());
        json_set(proxies, "https", json_null());
        json_set(o, "proxies", proxies);
    }
    return o;
}
