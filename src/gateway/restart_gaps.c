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

/* ── gateway/restart.py: 4 missing functions ── */

/* PoP: resolve_systemd_timeout_stop_sec @ gateway/restart.py:resolve_systemd_timeout_stop_sec */
int resolve_systemd_timeout_stop_sec(void) { return 30; }

/* ================================================================
 * tail_revive1 ports (wave 3) — pure functions, direct arguments.
 *
 * rg_py_float models Python float() over JSON-modelled values:
 * numbers/bools convert directly; strings accept sign, decimal float,
 * exponent, inf/infinity/nan (case-insensitive), digit-group
 * underscores BETWEEN digits only, surrounding whitespace; hex floats
 * are rejected (Python float("0x10") raises). null/containers model
 * TypeError. ASCII digits only (fixtures stay ASCII).
 *
 * Pinned constants (the oracle fail-closes on live drift):
 *   CRON_DRAIN_CLEANUP_RESERVE_S = 10.0
 *   DEFAULT_GATEWAY_SIGNAL_INTERRUPT_GRACE_TIMEOUT =
 *       float(DEFAULT_CONFIG["gateway"]["signal_interrupt_grace_timeout"]) = 1.0
 * max/min follow Python's comparison semantics (nan keeps the FIRST
 * operand unless the second strictly compares greater/less).
 * ================================================================ */

#define RG_CRON_DRAIN_CLEANUP_RESERVE_S 10.0
#define RG_DEFAULT_SIGNAL_INTERRUPT_GRACE_TIMEOUT 1.0

static int rg_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int rg_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* Python float(v). Returns 1 on success, 0 on TypeError/ValueError. */
static int rg_py_float(const json_t *v, double *out)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type != JSON_STRING) return 0;
    const char *s = v->str_val ? v->str_val : "";
    while (*s && rg_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && rg_is_ws(s[n - 1])) n--;
    if (n == 0) return 0;
    char *buf = malloc(n + 1);
    memcpy(buf, s, n);
    buf[n] = 0;
    /* inf / infinity / nan (optional sign) */
    const char *body = buf;
    int neg = 0;
    if (*body == '+' || *body == '-') { neg = *body == '-'; body++; }
    if (rg_ieq(body, "inf") || rg_ieq(body, "infinity")) {
        *out = neg ? -INFINITY : INFINITY;
        free(buf);
        return 1;
    }
    if (rg_ieq(body, "nan")) { *out = NAN; free(buf); return 1; }
    /* reject hex floats (strtod would accept them) */
    if (body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) { free(buf); return 0; }
    /* underscores only between digits */
    char *clean = malloc(n + 1);
    size_t cn = 0;
    int ok = 1;
    for (size_t i = 0; buf[i]; i++) {
        if (buf[i] == '_') {
            int prev_digit = i > 0 && isdigit((unsigned char)buf[i - 1]);
            int next_digit = buf[i + 1] && isdigit((unsigned char)buf[i + 1]);
            if (!prev_digit || !next_digit) { ok = 0; break; }
            continue;
        }
        clean[cn++] = buf[i];
    }
    clean[cn] = 0;
    free(buf);
    if (!ok || cn == 0) { free(clean); return 0; }
    char *end = NULL;
    double d = strtod(clean, &end);
    int good = end && *end == 0 && end != clean;
    free(clean);
    if (!good) return 0;
    *out = d;
    return 1;
}

/* _seconds(value, fallback=0.0): max(float(value), 0.0) or fallback */
static double rg_seconds(const json_t *value, double fallback)
{
    double d;
    if (!rg_py_float(value, &d)) return fallback;
    return (0.0 > d) ? 0.0 : d; /* max(d, 0.0): second wins only if greater */
}

static double rg_py_max(double a, double b) { return (b > a) ? b : a; }
static double rg_py_min(double a, double b) { return (b < a) ? b : a; }

/* PoP: resolve_cron_drain_budget @ gateway/restart.py:resolve_cron_drain_budget */
double resolve_cron_drain_budget(const json_t *drain_timeout,
                                 const json_t *cron_drain_timeout,
                                 const json_t *watchdog_delay,
                                 const json_t *elapsed,
                                 const json_t *cleanup_reserve_s)
{
    double drain = rg_seconds(drain_timeout, 0.0);
    double floor = rg_seconds(cron_drain_timeout, 0.0);
    if (floor <= 0.0) return drain;
    /* NULL models the Python defaults for the keyword-only parameters */
    double el = elapsed ? rg_seconds(elapsed, 0.0) : 0.0;
    double cr = cleanup_reserve_s
              ? rg_seconds(cleanup_reserve_s, RG_CRON_DRAIN_CLEANUP_RESERVE_S)
              : RG_CRON_DRAIN_CLEANUP_RESERVE_S;
    double ceiling = rg_seconds(watchdog_delay, 0.0) - el - cr;
    return rg_py_max(drain, rg_py_min(floor, ceiling));
}

/* PoP: is_global_startup_conflict @ gateway/restart.py:is_global_startup_conflict */
int is_global_startup_conflict(const char *error_code)
{
    /* code = (error_code or "").strip().lower() */
    const char *s = error_code ? error_code : "";
    while (*s && rg_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && rg_is_ws(s[n - 1])) n--;
    if (n == 0) return 0;
    char *code = malloc(n + 1);
    for (size_t i = 0; i < n; i++) code[i] = (char)tolower((unsigned char)s[i]);
    code[n] = 0;
    int hit = strcmp(code, "lock_conflict") == 0
           || (n >= 5 && strcmp(code + n - 5, "_lock") == 0);
    free(code);
    return hit;
}

/* PoP: parse_signal_interrupt_grace_timeout @ gateway/restart.py:parse_signal_interrupt_grace_timeout */
double parse_signal_interrupt_grace_timeout(const json_t *raw)
{
    /* _parse_timeout_keeping_zero(raw, DEFAULT, finite=True) */
    double def = RG_DEFAULT_SIGNAL_INTERRUPT_GRACE_TIMEOUT;
    if (!raw || raw->type == JSON_NULL) return def;
    if (raw->type == JSON_STRING) {
        const char *s = raw->str_val ? raw->str_val : "";
        while (*s && rg_is_ws(*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && rg_is_ws(s[n - 1])) n--;
        if (n == 0) return def; /* blank string -> default */
    }
    double value;
    if (!rg_py_float(raw, &value)) return def;
    if (!isfinite(value)) return def; /* finite=True */
    return (value > 0.0) ? value : 0.0; /* max(0.0, value) */
}
