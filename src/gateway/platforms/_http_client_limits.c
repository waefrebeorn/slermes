/*
 * _http_client_limits.c -- real C11 port of the env-tuned httpx limits of
 * upstream Hermes gateway/platforms/_http_client_limits.py.
 *
 * PoP: w22_httpclient_5f808a_positive_env_int @ gateway/platforms/_http_client_limits.py:_positive_env
 * PoP: w22_httpclient_5f808a_positive_env_float @ gateway/platforms/_http_client_limits.py:_positive_env
 * PoP: w22_httpclient_5f808a_limits @ gateway/platforms/_http_client_limits.py:platform_httpx_limits
 *
 * Seams: the env read (the caller hands the raw value in) and httpx being
 * importable (`have_httpx`). The parsing rule -- strip, cast, and keep the
 * default unless the value is > 0 (>= 0 for the keepalive count) -- is the
 * live rule, spelled the way CPython's int()/float() accept a string.
 */
#include "hermes_json.h"
#include "noc_w22_httpclient_5f808a.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HP_DEFAULT_EXPIRY 2.0
#define HP_DEFAULT_KEEPALIVE 10

static int hp_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* `str.strip()` over the ASCII whitespace CPython strips here. */
static void hp_trim(const char *raw, const char **p, size_t *len) {
    const char *s = raw ? raw : "";
    size_t n;
    n = strlen(s);
    while (n > 0 && hp_is_space(s[n - 1])) n--;
    while (n > 0 && hp_is_space(*s)) { s++; n--; }
    *p = s;
    *len = n;
}

static int hp_is_digit(char c) { return c >= '0' && c <= '9'; }

/* CPython int(): one optional sign, ASCII digits, single underscores only
 * between digits. Returns 0 when the text is not an int (ValueError) or does
 * not fit an int64 (the C ABI's integer width). */
static int hp_parse_int(const char *raw, long long *out) {
    const char *p;
    size_t n, i;
    int neg = 0, digits = 0;
    unsigned long long acc = 0;

    hp_trim(raw, &p, &n);
    if (n == 0) return 0;
    i = 0;
    if (p[0] == '+' || p[0] == '-') { neg = (p[0] == '-'); i = 1; }
    for (; i < n; i++) {
        char c = p[i];
        if (c == '_') {
            if (digits == 0 || i + 1 >= n || !hp_is_digit(p[i - 1]) || !hp_is_digit(p[i + 1])) return 0;
            continue;
        }
        if (!hp_is_digit(c)) return 0;
        if (acc > (9223372036854775807ULL - (unsigned)(c - '0')) / 10ULL) return 0;
        acc = acc * 10ULL + (unsigned)(c - '0');
        digits++;
    }
    if (digits == 0) return 0;
    if (neg) {
        if (acc > 9223372036854775808ULL) return 0;
        *out = (long long)(-(long long)(acc - 1) - 1);
    } else {
        *out = (long long)acc;
    }
    return 1;
}

/* CPython float(): strips, rejects a hex prefix, drops single underscores
 * between digits, then requires the whole remainder to spell one float. */
static int hp_parse_float(const char *raw, double *out) {
    const char *p;
    size_t n, i, k = 0;
    char clean[512];
    char *end = NULL;
    double val;

    hp_trim(raw, &p, &n);
    if (n == 0 || n >= sizeof(clean)) return 0;
    if (n >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) return 0;   /* no hex floats */
    for (i = 0; i < n; i++) {
        char c = p[i];
        if (c == '_') {
            if (i == 0 || i + 1 >= n || !hp_is_digit(p[i - 1]) || !hp_is_digit(p[i + 1])) return 0;
            continue;
        }
        clean[k++] = c;
    }
    clean[k] = '\0';
    if (k == 0) return 0;
    errno = 0;
    val = strtod(clean, &end);
    if (end == clean || !end || *end != '\0') return 0;
    *out = val;
    return 1;
}

long long w22_httpclient_5f808a_positive_env_int(const char *raw, long long def, int allow_zero) {
    long long val = 0;
    if (!hp_parse_int(raw, &val)) return def;
    if (val > 0 || (allow_zero && val == 0)) return val;
    return def;
}

double w22_httpclient_5f808a_positive_env_float(const char *raw, double def) {
    double val = 0.0;
    if (!hp_parse_float(raw, &val)) return def;
    return val > 0.0 ? val : def;
}

json_t *w22_httpclient_5f808a_limits(const char *raw_max_keepalive, const char *raw_expiry,
                                     int have_httpx) {
    json_t *out;
    long long keepalive;
    double expiry;
    if (!have_httpx) return json_null();
    keepalive = w22_httpclient_5f808a_positive_env_int(raw_max_keepalive, HP_DEFAULT_KEEPALIVE, 1);
    expiry = w22_httpclient_5f808a_positive_env_float(raw_expiry, HP_DEFAULT_EXPIRY);
    out = json_object();
    /* max_connections stays at the httpx default (100): not a tuned field. */
    json_set(out, "max_keepalive_connections", json_number((double)keepalive));
    json_set(out, "keepalive_expiry", json_number(expiry));
    return out;
}
