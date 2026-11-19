#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/web_result_cache.py: 15 missing functions ── */

/* PoP: web_config @ tools/web_result_cache.py:_web_config */
char *web_config(void) { return strdup("{}"); }

/* PoP: bucket_limit @ tools/web_result_cache.py:bucket_limit */
int bucket_limit(void) { return 100; }

/* PoP: normalize_query @ tools/web_result_cache.py:normalize_query */
char *normalize_query(const char *query) { return query ? strdup(query) : strdup(""); }

/* PoP: slice_search_response @ tools/web_result_cache.py:slice_search_response */
char *slice_search_response(const char *response) { return strdup("[]"); }

/* PoP: _cache_dir @ tools/web_result_cache.py:_cache_dir */
char *_cache_dir(void) { return strdup(""); }

/* PoP: _load_index @ tools/web_result_cache.py:_load_index */
char *_load_index(void) { return strdup("{}"); }

/* PoP: url_digest @ tools/web_result_cache.py:_url_digest */
char *url_digest(const char *url) { return strdup(""); }

/* PoP: entry_file_path @ tools/web_result_cache.py:_entry_file_path */
char *entry_file_path(const char *digest) { return strdup(""); }

/* is_local_dev_url: deferred (ipaddress classification surface). */
int is_local_dev_url(const char *url) { return 0; }

/* PoP: extract_cache_get @ tools/web_result_cache.py:extract_cache_get */
char *extract_cache_get(const char *url) { return strdup("{}"); }

/* PoP: extract_cache_put @ tools/web_result_cache.py:extract_cache_put */
void extract_cache_put(const char *url, const char *data) { (void)url; (void)data; }

/* ================================================================
 * tail_revive1 ports (wave 3). Seam contract as in
 * context_compressor_gaps.c. Seam used here:
 *   "web_config" -> _web_config(): the web config dict.
 * _host_matches_pattern is a pure sibling called directly (the oracle
 * extracts the live sibling, not a stub).
 *
 * wrcg_hostname models urllib.parse.urlparse(...).hostname exactly for
 * the exercised surface: WHATWG \t\r\n removal + leading/trailing C0
 * strip, scheme:// or // netloc detection, userinfo rsplit('@'),
 * bracketed IPv6/IPvFuture validation (ValueError "Invalid IPv6 URL"),
 * port split WITHOUT port validation, lowercasing, empty -> None.
 * Trailing dots are PRESERVED (raw urlparse does not rstrip('.')).
 * lower()/strip() are ASCII-faithful; fixtures stay ASCII.
 * ================================================================ */

#ifndef TAIL1_SEAM_FN_DEFINED
#define TAIL1_SEAM_FN_DEFINED
typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

static tail1_seam_fn wrcg_seam_fn = NULL;
static void *wrcg_seam_ctx = NULL;

void wrcg_seams_install(tail1_seam_fn fn, void *ctx)
{
    wrcg_seam_fn = fn;
    wrcg_seam_ctx = ctx;
}

static int wrcg_jtruth(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int wrcg_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static int wrcg_is_base_exc(const json_t *v)
{
    if (!wrcg_is_raise(v)) return 0;
    const char *k = json_obj_get(v, "__raise__")->str_val;
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *wrcg_seam(const char *name, json_t *args)
{
    json_t *r;
    if (!wrcg_seam_fn) {
        r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        json_set(r, "msg", json_string("tail1 seam not installed"));
    } else {
        r = wrcg_seam_fn(wrcg_seam_ctx, name, args);
    }
    json_free(args);
    return r;
}

/* ---- string helpers ---- */

static char *wrcg_lower_dup(const char *s)
{
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

static int wrcg_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* strip() both ends, in place on a malloc'd buffer */
static void wrcg_strip_ws(char *s)
{
    size_t n = strlen(s);
    size_t a = 0;
    while (a < n && wrcg_is_ws(s[a])) a++;
    size_t b = n;
    while (b > a && wrcg_is_ws(s[b - 1])) b--;
    memmove(s, s + a, b - a);
    s[b - a] = 0;
}

/* strip(chars) both ends */
static void wrcg_strip_chars(char *s, const char *chars)
{
    size_t n = strlen(s);
    size_t a = 0;
    while (a < n && strchr(chars, s[a])) a++;
    size_t b = n;
    while (b > a && strchr(chars, s[b - 1])) b--;
    memmove(s, s + a, b - a);
    s[b - a] = 0;
}

/* ---- Python str() rendering (scalars + containers) ---- */

static void wrcg_sb(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        *cap = (*len + n + 1) * 2;
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, n + 1);
    *len += n;
}

static void wrcg_float_repr(char *out, size_t outsz, double x)
{
    if (isnan(x)) { snprintf(out, outsz, "nan"); return; }
    if (isinf(x)) { snprintf(out, outsz, x < 0 ? "-inf" : "inf"); return; }
    if (x == 0.0) { snprintf(out, outsz, signbit(x) ? "-0.0" : "0.0"); return; }
    int p;
    char tmp[64];
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof tmp, "%.*g", p, x);
        if (strtod(tmp, NULL) == x) break;
    }
    if (p > 17) p = 17;
    snprintf(tmp, sizeof tmp, "%.*e", p - 1, x);
    int exp = atoi(strchr(tmp, 'e') + 1);
    char digits[40];
    size_t dn = 0;
    int neg = 0;
    for (const char *c = tmp; *c && *c != 'e'; c++) {
        if (*c == '-') { neg = 1; continue; }
        if (*c == '.') continue;
        digits[dn++] = *c;
    }
    digits[dn] = 0;
    while (dn > 1 && digits[dn - 1] == '0') digits[--dn] = 0;
    char body[96];
    if (exp >= -4 && exp < 16) {
        size_t o = 0;
        if (neg) body[o++] = '-';
        if (exp >= 0) {
            int ip = exp + 1;
            for (int i = 0; i < ip; i++)
                body[o++] = (i < (int)dn) ? digits[i] : '0';
            body[o++] = '.';
            if ((int)dn > ip) {
                for (int i = ip; i < (int)dn; i++) body[o++] = digits[i];
            } else {
                body[o++] = '0';
            }
        } else {
            body[o++] = '0'; body[o++] = '.';
            for (int i = 0; i < -exp - 1; i++) body[o++] = '0';
            for (int i = 0; i < (int)dn; i++) body[o++] = digits[i];
        }
        body[o] = 0;
    } else {
        char mant[48];
        size_t o = 0;
        mant[o++] = digits[0];
        if (dn > 1) {
            mant[o++] = '.';
            for (size_t i = 1; i < dn; i++) mant[o++] = digits[i];
        }
        mant[o] = 0;
        snprintf(body, sizeof body, "%s%se%s%02d",
                 neg ? "-" : "", mant, exp < 0 ? "-" : "+",
                 exp < 0 ? -exp : exp);
    }
    snprintf(out, outsz, "%s", body);
}

static void wrcg_repr_str(char **buf, size_t *len, size_t *cap, const char *s)
{
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    char tmp[2] = { q, 0 };
    wrcg_sb(buf, len, cap, tmp);
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        char esc[8];
        if (*c == '\\') wrcg_sb(buf, len, cap, "\\\\");
        else if (*c == '\n') wrcg_sb(buf, len, cap, "\\n");
        else if (*c == '\r') wrcg_sb(buf, len, cap, "\\r");
        else if (*c == '\t') wrcg_sb(buf, len, cap, "\\t");
        else if (*c == (unsigned char)q) {
            esc[0] = '\\'; esc[1] = (char)q; esc[2] = 0;
            wrcg_sb(buf, len, cap, esc);
        } else if (*c < 0x20 || *c == 0x7f) {
            snprintf(esc, sizeof esc, "\\x%02x", *c);
            wrcg_sb(buf, len, cap, esc);
        } else {
            tmp[0] = (char)*c; tmp[1] = 0;
            wrcg_sb(buf, len, cap, tmp);
        }
    }
    tmp[0] = q; tmp[1] = 0;
    wrcg_sb(buf, len, cap, tmp);
}

static void wrcg_render(char **buf, size_t *len, size_t *cap, const json_t *v, int repr_mode)
{
    char tmp[64];
    if (!v || v->type == JSON_NULL) { wrcg_sb(buf, len, cap, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:
        wrcg_sb(buf, len, cap, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER: {
        double x = v->num_val;
        if (x == floor(x) && fabs(x) < 1e16) {
            snprintf(tmp, sizeof tmp, "%.0f", x);
            wrcg_sb(buf, len, cap, tmp);
        } else {
            wrcg_float_repr(tmp, sizeof tmp, x);
            wrcg_sb(buf, len, cap, tmp);
        }
        return;
    }
    case JSON_STRING:
        if (repr_mode) wrcg_repr_str(buf, len, cap, v->str_val ? v->str_val : "");
        else wrcg_sb(buf, len, cap, v->str_val ? v->str_val : "");
        return;
    case JSON_ARRAY:
        wrcg_sb(buf, len, cap, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) wrcg_sb(buf, len, cap, ", ");
            wrcg_render(buf, len, cap, v->c.items[i], 1);
        }
        wrcg_sb(buf, len, cap, "]");
        return;
    case JSON_OBJECT:
        wrcg_sb(buf, len, cap, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) wrcg_sb(buf, len, cap, ", ");
            wrcg_repr_str(buf, len, cap, v->c.keys[i]);
            wrcg_sb(buf, len, cap, ": ");
            wrcg_render(buf, len, cap, v->c.items[i], 1);
        }
        wrcg_sb(buf, len, cap, "}");
        return;
    default:
        return;
    }
}

static char *wrcg_py_str(const json_t *v)
{
    char *buf = malloc(16);
    size_t len = 0, cap = 16;
    buf[0] = 0;
    wrcg_render(&buf, &len, &cap, v, 0);
    return buf;
}

/* ---- urlparse(...).hostname ---- */

/* Strict IPv4 dotted quad (no leading zeros, octets <= 255). */
static int wrcg_ipv4_ok(const char *s)
{
    int octets = 0;
    const char *p = s;
    for (;;) {
        if (!isdigit((unsigned char)*p)) return 0;
        const char *start = p;
        int val = 0;
        while (isdigit((unsigned char)*p)) {
            val = val * 10 + (*p - '0');
            if (p - start >= 3) return 0;
            p++;
        }
        if (p - start > 1 && start[0] == '0') return 0;
        if (val > 255) return 0;
        octets++;
        if (!*p) break;
        if (*p != '.') return 0;
        p++;
    }
    return octets == 4;
}

/* Pure IPv6 group grammar; the IPv4 tail (when present) must already be
 * validated and replaced by "0:0" in the working copy. */
static int wrcg_ipv6_groups_ok(const char *s)
{
    int groups = 0, dbl = 0;
    const char *p = s;
    if (!*p) return 0;
    if (p[0] == ':') {
        if (p[1] != ':') return 0;
        dbl = 1;
        p += 2;
        if (!*p) return 1; /* "::" */
    }
    for (;;) {
        int d = 0;
        while (isxdigit((unsigned char)*p) && d < 4) { d++; p++; }
        if (d == 0) return 0;
        if (isxdigit((unsigned char)*p)) return 0; /* >4 hex digits */
        groups++;
        if (!*p) break;
        if (*p != ':') return 0;
        p++;
        if (*p == ':') {
            if (dbl) return 0;
            dbl = 1;
            p++;
            if (!*p) break;
        } else if (!*p) {
            return 0; /* trailing single colon */
        }
    }
    if (dbl) return groups <= 7; /* :: must fill at least one group */
    return groups == 8;
}

static int wrcg_ipv6_ok(const char *s)
{
    const char *last_colon = strrchr(s, ':');
    if (!last_colon) return 0;
    const char *tail = last_colon + 1;
    if (strchr(tail, '.')) {
        if (!wrcg_ipv4_ok(tail)) return 0;
        size_t plen = (size_t)(last_colon - s) + 1;
        char *copy = malloc(plen + 4);
        memcpy(copy, s, plen);
        memcpy(copy + plen, "0:0", 4);
        int ok = wrcg_ipv6_groups_ok(copy);
        free(copy);
        return ok;
    }
    if (strchr(s, '.')) return 0; /* dot outside the tail */
    return wrcg_ipv6_groups_ok(s);
}

/* _check_bracketed_host: IPvFuture regex or ip_address(); an IPv4 in
 * brackets is rejected. All failures are ValueError in live. */
static int wrcg_bracketed_host_ok(const char *h)
{
    if (h[0] == 'v' || h[0] == 'V') {
        size_t i = 1;
        while (isxdigit((unsigned char)h[i])) i++;
        if (i == 1) return 0;
        if (h[i] != '.') return 0;
        if (!h[i + 1]) return 0;
        for (const char *p = h + i + 1; *p; p++)
            if (*p == '\n') return 0;
        return 1;
    }
    if (wrcg_ipv4_ok(h)) return 0; /* IPv4 cannot be in brackets */
    /* optional scope zone: non-empty, no '%' inside */
    const char *pct = strchr(h, '%');
    if (pct) {
        if (!pct[1]) return 0;
        if (strchr(pct + 1, '%')) return 0;
        size_t n = (size_t)(pct - h);
        char *ip = malloc(n + 1);
        memcpy(ip, h, n);
        ip[n] = 0;
        int ok = wrcg_ipv6_ok(ip);
        free(ip);
        return ok;
    }
    return wrcg_ipv6_ok(h);
}

/* Returns malloc'd hostname or NULL (None). *err=1 models the ValueError
 * family urlparse raises on invalid bracketed netlocs; the live caller
 * catches it (except Exception -> False). Mirrors CPython _urlsplit +
 * _check_bracketed_netloc + _hostinfo + hostname:
 *  - remove \t\r\n anywhere, lstrip C0+space (trailing space PRESERVED)
 *  - scheme:// or // netloc; authority ends at /?#
 *  - bracket presence must balance across the WHOLE netloc
 *  - userinfo rsplit('@'); no data before '['; after ']' only ':' or end
 *  - bracketed host must be IPv6 (with scope) or IPvFuture; when the
 *    brackets sit in the userinfo, the bare host is validated the same way
 *  - hostname lowercases only the part BEFORE the first '%' (zone case kept)
 */
static char *wrcg_hostname(const char *url, int *err)
{
    *err = 0;
    if (!url) return NULL;
    size_t n = strlen(url);
    char *clean = malloc(n + 1);
    size_t cn = 0;
    for (size_t i = 0; i < n; i++) {
        char c = url[i];
        if (c == '\t' || c == '\r' || c == '\n') continue;
        clean[cn++] = c;
    }
    clean[cn] = 0;
    size_t a = 0;
    while (a < cn && (unsigned char)clean[a] <= 0x20) a++;
    char *u = clean + a;
    const char *nl = NULL;
    if (u[0] == '/' && u[1] == '/') {
        nl = u + 2;
    } else {
        const char *colon = strchr(u, ':');
        if (colon && colon > u && colon[1] == '/' && colon[2] == '/'
            && isalpha((unsigned char)u[0])) {
            int valid = 1;
            for (const char *c = u + 1; c < colon; c++) {
                if (!(isalnum((unsigned char)*c) || *c == '+' || *c == '-' || *c == '.')) {
                    valid = 0;
                    break;
                }
            }
            if (valid) nl = colon + 3;
        }
    }
    if (!nl) { free(clean); return NULL; }
    size_t alen = strcspn(nl, "/?#");
    char *auth = malloc(alen + 1);
    memcpy(auth, nl, alen);
    auth[alen] = 0;
    free(clean);
    char *host = NULL;
    int have_open = strchr(auth, '[') != NULL;
    int have_close = strchr(auth, ']') != NULL;
    if (have_open != have_close) {
        *err = 1;
        free(auth);
        return NULL;
    }
    char *at = strrchr(auth, '@');
    char *hp = at ? at + 1 : auth;
    if (have_open && have_close) {
        char *ob = strchr(hp, '[');
        if (ob) {
            if (ob != hp) { /* data before the bracket */
                *err = 1;
                free(auth);
                return NULL;
            }
            char *cb = strchr(ob + 1, ']');
            const char *hend;
            if (cb) {
                if (cb[1] != 0 && cb[1] != ':') {
                    *err = 1;
                    free(auth);
                    return NULL;
                }
                hend = cb;
            } else {
                hend = ob + 1 + strlen(ob + 1);
            }
            size_t hl = (size_t)(hend - (ob + 1));
            host = malloc(hl + 1);
            memcpy(host, ob + 1, hl);
            host[hl] = 0;
            if (!wrcg_bracketed_host_ok(host)) {
                *err = 1;
                free(host);
                free(auth);
                return NULL;
            }
        } else {
            /* brackets live in the userinfo: validate the bare host */
            char *colon = strchr(hp, ':');
            size_t hl = colon ? (size_t)(colon - hp) : strlen(hp);
            host = malloc(hl + 1);
            memcpy(host, hp, hl);
            host[hl] = 0;
            if (!wrcg_bracketed_host_ok(host)) {
                *err = 1;
                free(host);
                free(auth);
                return NULL;
            }
        }
    } else {
        char *colon = strchr(hp, ':');
        size_t hl = colon ? (size_t)(colon - hp) : strlen(hp);
        host = malloc(hl + 1);
        memcpy(host, hp, hl);
        host[hl] = 0;
    }
    free(auth);
    /* lowercase only before the first '%' (scoped IPv6 zone keeps case) */
    for (char *p = host; *p && *p != '%'; p++)
        *p = (char)tolower((unsigned char)*p);
    if (!host[0]) { free(host); return NULL; }
    return host;
}

/* ---- implemented ports ---- */

/* PoP: host_matches_pattern @ tools/web_result_cache.py:_host_matches_pattern */
int host_matches_pattern(const char *host, const char *pattern)
{
    /* host = host.lower().strip(".") */
    char *h = wrcg_lower_dup(host);
    wrcg_strip_chars(h, ".");
    /* pattern = (pattern or "").lower().strip().strip(".").removeprefix("*.") */
    char *p = wrcg_lower_dup(pattern); /* NULL -> "" models `pattern or ""` */
    wrcg_strip_ws(p);
    wrcg_strip_chars(p, ".");
    if (p[0] == '*' && p[1] == '.') {
        size_t pl = strlen(p);
        memmove(p, p + 2, pl - 1);
    }
    int hit = 0;
    if (p[0]) {
        if (strcmp(h, p) == 0) {
            hit = 1;
        } else {
            size_t hl = strlen(h), pl = strlen(p);
            if (hl >= pl + 1 && h[hl - pl - 1] == '.' && strcmp(h + hl - pl, p) == 0)
                hit = 1;
        }
    }
    free(h);
    free(p);
    return hit;
}

/* PoP: is_cache_exempt_host @ tools/web_result_cache.py:_is_cache_exempt_host */
json_t *is_cache_exempt_host(const char *url)
{
    json_t *cfg = wrcg_seam("web_config", json_array());
    if (wrcg_is_raise(cfg)) {
        if (wrcg_is_base_exc(cfg)) return cfg;
        json_free(cfg);
        return json_bool(false); /* except Exception: return False */
    }
    /* _web_config().get(...) — non-dict raises AttributeError (caught) */
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_free(cfg);
        return json_bool(false);
    }
    json_t *patterns = json_obj_get(cfg, "cache_exempt_hosts"); /* or [] below */
    int herr = 0;
    char *host = wrcg_hostname(url, &herr);
    if (herr) { /* urlparse ValueError -> except -> False */
        free(host);
        json_free(cfg);
        return json_bool(false);
    }
    if (!host) host = strdup("");
    wrcg_strip_chars(host, "[]");
    int result = 0;
    if (wrcg_jtruth(patterns) && patterns->type == JSON_ARRAY && host[0]) {
        for (size_t i = 0; i < patterns->c.count; i++) {
            char *sp = wrcg_py_str(patterns->c.items[i]); /* str(p) */
            if (host_matches_pattern(host, sp)) result = 1;
            free(sp);
            if (result) break; /* any() short-circuits */
        }
    }
    /* falsy patterns -> []; non-list/tuple truthy patterns -> False;
     * empty host -> False — all covered by the guard above. */
    free(host);
    json_free(cfg);
    return json_bool(result);
}
