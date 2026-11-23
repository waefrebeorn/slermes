/*
 * env.c — C port of upstream Hermes utils/env.py (gate w5_env_ee14b1).
 *
 * PoP: w5_env_ee14b1_is_truthy_value          @ utils/env.py:is_truthy_value
 * PoP: w5_env_ee14b1_normalize_proxy_url      @ utils/env.py:normalize_proxy_url
 * PoP: w5_env_ee14b1_bare_ipv6_literal        @ utils/env.py:_bare_ipv6_literal
 * PoP: w5_env_ee14b1_sanitize_no_proxy_entries @ utils/env.py:sanitize_no_proxy_entries
 * PoP: w5_env_ee14b1_env_int_ex               @ utils/env.py:env_int
 * PoP: w5_env_ee14b1_env_float_ex             @ utils/env.py:env_float
 *
 * All logic is pure data. The env_* helpers take the injected raw value so
 * no process environment is touched by the C side.
 */
#include "noc_w5_env_ee14b1.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void w5_env_ee14b1_free(char *s) { free(s); }

static char *dupn(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *dupstr(const char *s) { return dupn(s, s ? strlen(s) : 0); }

/* Python str.strip(): ASCII whitespace is enough for the fixture domain. */
static const char *strip(const char *s, size_t *len_out) {
    size_t a = 0, b;
    if (!s) { if (len_out) *len_out = 0; return ""; }
    b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (len_out) *len_out = b - a;
    return s + a;
}

static bool ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* length-bounded case-insensitive compare, for the stripped lower() set */
static int ieqn(const char *s, size_t n, const char *lit) {
    if (strlen(lit) != n) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != (unsigned char)lit[i]) return 0;
    return 1;
}

bool w5_env_ee14b1_is_truthy_value(w5_env_ee14b1_vkind_t kind,
                                   const char *str, double num, bool b, bool def) {
    if (kind == W5_ENV_EE14B1_NULL) return def;
    if (kind == W5_ENV_EE14B1_STRING) {
        size_t n = 0;
        const char *s = strip(str, &n);
        return (n == 1 && s[0] == '1')
            || ieqn(s, n, "true") || ieqn(s, n, "yes") || ieqn(s, n, "on");
    }
    if (kind == W5_ENV_EE14B1_NUMBER) return num != 0.0;
    return b;
}

char *w5_env_ee14b1_normalize_proxy_url(const char *proxy_url) {
    size_t n = 0;
    const char *s = strip(proxy_url ? proxy_url : "", &n);
    static const char pfx[] = "socks://";
    size_t pn = sizeof(pfx) - 1;
    int prefix = n >= pn;
    for (size_t i = 0; prefix && i < pn; i++)
        if (tolower((unsigned char)s[i]) != (unsigned char)pfx[i]) prefix = 0;
    if (prefix) {
        const char *rest = s + pn;
        size_t rn = n - pn;
        char *out = (char *)malloc(sizeof("socks5://") + rn);
        if (!out) return NULL;
        memcpy(out, "socks5://", sizeof("socks5://") - 1);
        if (rn) memcpy(out + sizeof("socks5://") - 1, rest, rn);
        out[sizeof("socks5://") - 1 + rn] = '\0';
        return out;
    }
    return n ? dupn(s, n) : NULL;
}

/* ---------- IPv6 literal detection (mirrors _bare_ipv6_literal) ---------- */

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int cls_ok(char c) {
    return hexv(c) >= 0 || c == ':' || c == '.';
}
static int valid_ipv4(const char *s, size_t n) {
    int parts = 0;
    size_t i = 0;
    while (i < n) {
        unsigned v = 0; size_t d = 0;
        if (parts == 4) return 0;
        while (i < n && isdigit((unsigned char)s[i])) {
            v = v * 10 + (unsigned)(s[i] - '0');
            if (v > 255) return 0;
            d++; i++;
        }
        if (!d || v > 255) return 0;
        parts++;
        if (i < n) {
            if (s[i] != '.') return 0;
            i++;
            if (i >= n) return 0;
        }
    }
    return parts == 4;
}
/* Parse a colon-free slice; record how many 16-bit groups it represents.
 * An embedded dotted quad is only legal as the final token. */
static int parse_groups(const char *s, size_t n, int allow_v4, int *groups) {
    size_t i = 0;
    *groups = 0;
    if (n == 0) return 1;
    while (i < n) {
        size_t j = i;
        while (j < n && s[j] != ':') j++;
        size_t len = j - i;
        if (!len) return 0;
        int last = (j == n);
        if (memchr(s + i, '.', len) != NULL) {
            if (!last || !allow_v4 || !valid_ipv4(s + i, len)) return 0;
            *groups += 2;
        } else {
            if (len > 4) return 0;
            for (size_t k = 0; k < len; k++)
                if (hexv(s[i + k]) < 0) return 0;
            *groups += 1;
        }
        if (!last) { i = j + 1; }
        else break;
    }
    return 1;
}
static int valid_ipv6(const char *s, size_t n) {
    size_t dc = 0;
    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] == ':' && s[i + 1] == ':') dc++;
    if (dc > 1) return 0;
    if (dc == 0) {
        int g;
        if (!parse_groups(s, n, 1, &g)) return 0;
        return g == 8;
    }
    size_t pos = 0;
    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] == ':' && s[i + 1] == ':') { pos = i; break; }
    int gl = 0, gr = 0;
    /* A dotted quad may live in the left half only when the right half is empty. */
    if (!parse_groups(s, pos, (n == pos + 2), &gl)) return 0;
    if (!parse_groups(s + pos + 2, n - (pos + 2), 1, &gr)) return 0;
    return (gl + gr) <= 7;
}

char *w5_env_ee14b1_bare_ipv6_literal(const char *entry) {
    size_t n = 0;
    const char *s = strip(entry ? entry : "", &n);

    /* re.fullmatch(r"\[([0-9A-Fa-f:.]+)\](?:[/:].*)?", candidate) */
    if (n >= 3 && s[0] == '[') {
        size_t j = 1;
        while (j < n && s[j] != ']') { if (!cls_ok(s[j])) break; j++; }
        if (j > 1 && j < n && s[j] == ']') {
            int all = 1;
            for (size_t k = 1; k < j; k++) if (!cls_ok(s[k])) { all = 0; break; }
            char rest = s[j + 1];
            if (all && (rest == '\0' || rest == '/' || rest == ':')
                && valid_ipv6(s + 1, j - 1))
                return dupn(s + 1, j - 1);
        }
    }
    /* re.fullmatch(r"([0-9A-Fa-f:.]+)/\d+", candidate) */
    if (n >= 3) {
        size_t k = 0;
        while (k < n && cls_ok(s[k])) k++;
        if (k > 0 && k < n && s[k] == '/') {
            size_t d = k + 1;
            while (d < n && isdigit((unsigned char)s[d])) d++;
            if (d == n && valid_ipv6(s, k)) return dupn(s, k);
        }
    }
    return NULL;
}

char *w5_env_ee14b1_sanitize_no_proxy_entries(const char *no_proxy_value) {
    const char *raw = no_proxy_value ? no_proxy_value : "";
    char buf[16384];
    size_t bn = 0;
    const char *tokens[512];
    size_t nt = 0;
    size_t rn = strlen(raw), i = 0;
    while (i < rn) {
        while (i < rn && (isspace((unsigned char)raw[i]) || raw[i] == ',')) i++;
        size_t a = i;
        while (i < rn && !isspace((unsigned char)raw[i]) && raw[i] != ',') i++;
        if (i > a && nt < 512) tokens[nt++] = dupn(raw + a, i - a);
    }
    if (nt == 0) return dupstr(raw);
    for (size_t t = 0; t < nt; t++) {
        if (strcmp(tokens[t], "*") == 0) {
            for (size_t k = 0; k < nt; k++) free((void *)tokens[k]);
            return dupstr(raw);
        }
    }

    const char *out[512];
    int out_bare[512];
    size_t no = 0;
    for (size_t t = 0; t < nt; t++) {
        char *bare = w5_env_ee14b1_bare_ipv6_literal(tokens[t]);
        if (!bare) {
            out[no] = tokens[t]; out_bare[no] = 0; no++;
        } else {
            int seen = 0;
            for (size_t o = 0; o < no; o++)
                if (strcmp(out[o], bare) == 0) { seen = 1; break; }
            if (seen) free(bare);
            else { out[no] = bare; out_bare[no] = 1; no++; }
        }
    }
    int same = (no == nt);
    if (same)
        for (size_t t = 0; t < nt; t++)
            if (strcmp(tokens[t], out[t]) != 0) { same = 0; break; }

    char *result;
    if (same) {
        result = dupstr(raw);
    } else {
        for (size_t o = 0; o < no; o++) {
            if (bn) buf[bn++] = ',';
            size_t l = strlen(out[o]);
            if (bn + l >= sizeof(buf)) l = sizeof(buf) - 1 - bn;
            memcpy(buf + bn, out[o], l); bn += l;
        }
        buf[bn] = '\0';
        result = dupstr(buf);
    }
    for (size_t k = 0; k < nt; k++) free((void *)tokens[k]);
    for (size_t o = 0; o < no; o++) if (out_bare[o]) free((void *)out[o]);
    return result;
}

long long w5_env_ee14b1_env_int_ex(const char *raw, long long def) {
    size_t n = 0;
    const char *s = strip(raw ? raw : "", &n);
    if (!n) return def;
    char *tmp = dupn(s, n);
    if (!tmp) return def;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(tmp, &end, 10);
    if (errno || end != tmp + n) { free(tmp); return def; }
    free(tmp);
    return v;
}

double w5_env_ee14b1_env_float_ex(const char *raw, double def) {
    size_t n = 0;
    const char *s = strip(raw ? raw : "", &n);
    if (!n) return def;
    char *tmp = dupn(s, n);
    if (!tmp) return def;
    errno = 0;
    char *end = NULL;
    double v = strtod(tmp, &end);
    if (errno || end != tmp + n) { free(tmp); return def; }
    free(tmp);
    return v;
}
