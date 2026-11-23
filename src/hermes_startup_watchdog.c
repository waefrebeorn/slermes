/*
 * hermes_startup_watchdog.c — faithful C11 port of the pure decision seams of
 * upstream Hermes hermes_startup_watchdog.py (gate w5_hermesstart_a84a61).
 *
 * PoP: w5_hermesstart_a84a61_disabled @ hermes_startup_watchdog.py:startup_watchdog_disabled
 * PoP: w5_hermesstart_a84a61_resolve_timeout @ hermes_startup_watchdog.py:resolve_startup_watchdog_timeout
 * PoP: w5_hermesstart_a84a61_dump_path @ hermes_startup_watchdog.py:get_startup_watchdog_dump_path
 * PoP: w5_hermesstart_a84a61_kick_extra @ hermes_startup_watchdog.py:StartupWatchdogHandle.kick
 * PoP: w5_hermesstart_a84a61_lease_clamp @ hermes_startup_watchdog.py:StartupWatchdogHandle.lease
 * PoP: w5_hermesstart_a84a61_arm_timeout @ hermes_startup_watchdog.py:arm_startup_watchdog
 *
 * Env values are injected per case; no process environment is mutated.
 */
#include "noc_w5_hermesstart_a84a61.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W5H_DEFAULT_TIMEOUT 300.0
#define W5H_MIN_TIMEOUT 30.0
#define W5H_MAX_LEASE 900.0

static char *w5h_strip_dup(const char *s) {
    size_t a, b, n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    a = 0; b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* Faithful float() on the stripped string: rejects empty and trailing junk. */
static int w5h_parse_double(const char *s, double *out) {
    char *t = w5h_strip_dup(s);
    char *end;
    double v;
    if (!t) return 0;
    if (!*t) { free(t); return 0; }
    v = strtod(t, &end);
    if (end == t) { free(t); return 0; }
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) { free(t); return 0; }
    free(t);
    *out = v;
    return 1;
}

struct json_t *w5_hermesstart_a84a61_disabled(const char *env_value) {
    char *t = w5h_strip_dup(env_value);
    int off;
    if (!t) return json_bool(0);
    for (char *p = t; *p; p++) *p = (char)tolower((unsigned char)*p);
    off = (!strcmp(t, "0") || !strcmp(t, "false") || !strcmp(t, "no") || !strcmp(t, "off"));
    free(t);
    return json_bool(off ? 1 : 0);
}

static double w5h_resolve_d(const char *raw) {
    double v;
    if (!w5h_parse_double(raw, &v)) return W5H_DEFAULT_TIMEOUT;
    if (v <= 0.0) return W5H_DEFAULT_TIMEOUT;
    return v < W5H_MIN_TIMEOUT ? W5H_MIN_TIMEOUT : v;
}

struct json_t *w5_hermesstart_a84a61_resolve_timeout(const char *raw) {
    return json_number(w5h_resolve_d(raw));
}

struct json_t *w5_hermesstart_a84a61_dump_path(const char *home) {
    char path[8192];
    int rc;
    if (!home) home = "";
    rc = snprintf(path, sizeof(path), "%s/logs/gateway-startup-watchdog.log", home);
    if (rc < 0 || (size_t)rc >= sizeof(path)) return json_null();
    return json_string(path);
}

struct json_t *w5_hermesstart_a84a61_kick_extra(const char *extra_s) {
    double v;
    if (!w5h_parse_double(extra_s, &v)) return json_number(0.0);
    return json_number(v < 0.0 ? 0.0 : v);
}

struct json_t *w5_hermesstart_a84a61_lease_clamp(const char *expected_s) {
    double v;
    if (!w5h_parse_double(expected_s, &v)) return json_null();
    if (v <= 0.0) return json_null();
    return json_number(v > W5H_MAX_LEASE ? W5H_MAX_LEASE : v);
}

struct json_t *w5_hermesstart_a84a61_arm_timeout(const char *timeout_s,
                                                 int disabled, const char *env_raw) {
    double resolved;
    if (disabled) return json_null();
    resolved = w5h_resolve_d(env_raw);
    if (timeout_s != NULL) {
        double v;
        if (!w5h_parse_double(timeout_s, &v)) return json_null();
        if (v > 0.0) resolved = v;
    }
    return json_number(resolved);
}
