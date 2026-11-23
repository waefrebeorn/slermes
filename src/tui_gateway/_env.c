/*
 * _env.c — C port of upstream Hermes tui_gateway/_env.py.
 * PoP: tenv_env_int @ tui_gateway/_env.py:env_int
 * PoP: tenv_env_float @ tui_gateway/_env.py:env_float
 * PoP: tenv_env_number @ tui_gateway/_env.py:_env_number
 *
 * Python semantics mirrored: cast(os.environ.get(name, "") or default) with
 * (TypeError, ValueError) -> default. Only the ASCII subset of str.strip,
 * int() and float() is modelled (all fixtures/quarry knobs are ASCII).
 */
#include "noc_tenv.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

static int is_ws(char c) {
    return c == ' ' || (c >= '\t' && c <= '\r'); /* \t \n \v \f \r */
}

/* _env_number's "os.environ.get(name,'') or default" gate: an absent or
 * EMPTY env value is falsy, so the caller must use its default. */
static const char *env_raw(const char *name) {
    const char *v = getenv(name);
    return (v && v[0]) ? v : NULL;
}

/* Python int(str): leading/trailing whitespace, optional sign, single
 * underscores between digits; nothing else (ValueError on invalid input). */
static int py_int(const char *s, int64_t *out) {
    while (is_ws(*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && is_ws(e[-1])) e--;
    if (e == s) return 0;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (s == e) return 0;
    char buf[64];
    size_t n = 0;
    int prev_digit = 0, ends_digit = 0;
    for (const char *p = s; p < e; p++) {
        if (*p >= '0' && *p <= '9') {
            if (n + 1 < sizeof(buf)) buf[n++] = *p;
            prev_digit = 1; ends_digit = 1;
        } else if (*p == '_') {
            if (!prev_digit) return 0;      /* no leading/double underscore */
            prev_digit = 0; ends_digit = 0; /* trailing underscore rejected */
        } else {
            return 0;
        }
    }
    if (!ends_digit || n == 0) return 0;
    buf[n] = '\0';
    errno = 0;
    char *ep = NULL;
    long long v = strtoll(buf, &ep, 10);
    if (*ep != '\0') return 0;
    *out = neg ? -v : v; /* ERANGE saturates like C long long; fixtures stay in range */
    return 1;
}

/* Python float(str): whitespace, sign, underscores between digits,
 * decimal/scientific/inf/nan; reject C-only hex floats and trailing junk. */
static int py_float(const char *s, double *out) {
    while (is_ws(*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && is_ws(e[-1])) e--;
    if (e == s) return 0;
    size_t len = (size_t)(e - s);
    char buf[512];
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '_') {
            if (i == 0 || i + 1 == len || !isdigit((unsigned char)s[i - 1]) ||
                !isdigit((unsigned char)s[i + 1])) return 0;
            continue;
        }
        if (n + 1 < sizeof(buf)) buf[n++] = c;
    }
    buf[n] = '\0';
    const char *q = buf;
    if (*q == '+' || *q == '-') q++;
    if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) return 0;
    char *ep = NULL;
    double v = strtod(buf, &ep);
    if (ep == buf || *ep != '\0') return 0;
    *out = v;
    return 1;
}

int64_t tenv_env_int(const char *name, int64_t default_val) {
    const char *raw = env_raw(name);
    int64_t v;
    if (!raw || !py_int(raw, &v)) return default_val;
    return v;
}

double tenv_env_float(const char *name, double default_val) {
    const char *raw = env_raw(name);
    double v;
    if (!raw || !py_float(raw, &v)) return default_val;
    return v;
}
