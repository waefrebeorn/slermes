/*
 * version_info.c — C home for upstream Hermes hermes_cli/version_info.py.
 *
 * PoP: w9_versioninfo_941aa7_derived_version @ hermes_cli/version_info.py:_derived_version
 * PoP: w9_versioninfo_941aa7_parse_nonnegative @ hermes_cli/version_info.py:_parse_nonnegative
 * PoP: w9_versioninfo_941aa7_display_version @ hermes_cli/version_info.py:VersionInfo.display_version
 *
 * The git/stamp resolution layers are injected; the pure version-string and
 * integer-parsing decisions are ported verbatim.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w9_versioninfo_941aa7.h"

/* Strict decimal parse mirroring Python int(str) for the values the callers
 * pass (optional sign, surrounding ASCII whitespace).  NULL sentinel on bad
 * input; *ok distinguishes "no value" from a parsed 0. */
static long long parse_ll(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *start = s;
    if (*s == '+' || *s == '-') s++;
    const char *digits = s;
    while (*s >= '0' && *s <= '9') s++;
    if (s == digits) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s || s == start) return 0;
    char buf[64];
    size_t n = (size_t)(s - start);
    if (n >= sizeof(buf)) return 0;
    memcpy(buf, start, n);
    buf[n] = '\0';
    /* strtoll stops at trailing whitespace; Python int() permits it.  Trim it
     * so the "no junk after digits" check compares the real digit run. */
    while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
    if (n == 0) return 0;
    char *end = NULL;
    long long v = strtoll(buf, &end, 10);
    if (end != buf + n) return 0;
    *ok = 1;
    return v;
}

/* "" encodes None for the optional distance/short_commit arguments. */
static int opt_ll(const char *s, long long *out) {
    if (!s || !*s) return 0;
    int ok = 0;
    long long v = parse_ll(s, &ok);
    if (!ok) return 0; /* non-numeric string is treated as absent, like None */
    *out = v;
    return 1;
}

static json_t *derived(const char *base, const char *distance_str, int dirty,
                       const char *short_commit) {
    long long dist = 0;
    int has_dist = opt_ll(distance_str, &dist);
    int has_short = short_commit && *short_commit;
    char out[512];
    if (has_dist && dist > 0) {
        if (has_short)
            snprintf(out, sizeof(out), "%s+%lld.g%s%s", base, dist, short_commit,
                     (dirty && has_short) ? ".dirty" : "");
        else
            snprintf(out, sizeof(out), "%s+%lld%s", base, dist,
                     (dirty && has_short) ? ".dirty" : "");
        return json_string(out);
    }
    if (dirty && !has_dist) {
        snprintf(out, sizeof(out), "%s+?", base);
        return json_string(out);
    }
    return json_string(base ? base : "");
}

struct json_t *w9_versioninfo_941aa7_derived_version(const char *base_version,
                                                     const char *distance_str,
                                                     int dirty,
                                                     const char *short_commit) {
    return derived(base_version, distance_str, dirty, short_commit);
}

struct json_t *w9_versioninfo_941aa7_parse_nonnegative(const char *value) {
    int ok = 0;
    long long v = parse_ll(value && *value ? value : "", &ok);
    if (!ok || v < 0) return json_null();
    return json_number((double)v);
}

struct json_t *w9_versioninfo_941aa7_display_version(const char *base_version,
                                                     const char *derived_version,
                                                     const char *distance_str) {
    if (base_version && !strcmp(base_version, "unknown"))
        return json_string(derived_version ? derived_version : "");
    return derived(base_version, distance_str, 0, NULL);
}

void hermes_cli_version_info_port_pending(void) { (void)0; }
