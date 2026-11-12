/*
 * port_version_info.c — Port of the pure half of hermes_cli/version_info.py.
 *
 * Identity derivation and parsing. The git/subprocess/install-stamp half of
 * the Python module does not port: it is I/O, and the C tree already resolves
 * its own provenance. What matters here is that every consumer — startup
 * banner, update receipt, gateway status stamp, outbound User-Agent — formats
 * a version the same way.
 */

#include "port_version_info.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION_UNKNOWN "unknown"

/* PoP: _derived_version @ hermes_cli/version_info.py:_derived_version */
void version_derived(const char *base_version, long distance, int has_distance,
                     int dirty, const char *short_commit, char *out,
                     size_t outsz)
{
    if (!out || outsz == 0)
        return;
    const char *base = base_version ? base_version : VERSION_UNKNOWN;
    const char *sha = (short_commit && *short_commit) ? short_commit : NULL;

    /* `if distance and distance > 0` — 0 and None are both falsy. */
    if (has_distance && distance > 0) {
        if (sha)
            snprintf(out, outsz, "%s+%ld.g%s%s", base, distance, sha,
                     (dirty && sha) ? ".dirty" : "");
        else
            snprintf(out, outsz, "%s+%ld", base, distance);
        return;
    }
    if (dirty && !has_distance) {
        snprintf(out, outsz, "%s+?", base);
        return;
    }
    snprintf(out, outsz, "%s", base);
}

/* PoP: _parse_nonnegative @ hermes_cli/version_info.py:_parse_nonnegative */
long version_parse_nonnegative(const char *value, int *ok)
{
    if (!ok)
        return 0;
    *ok = 0;
    if (!value)
        return 0;
    /* int(value or "") — a None/empty input raises ValueError -> None. */
    while (isspace((unsigned char)*value))
        value++;
    if (!*value)
        return 0;
    errno = 0;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value)
        return 0;
    /* Python's int() ignores surrounding whitespace, but not other junk. */
    while (isspace((unsigned char)*end))
        end++;
    if (*end)
        return 0;
    if (parsed < 0 || parsed > LONG_MAX)
        return 0;
    *ok = 1;
    return parsed;
}

/* PoP: VersionInfo.display_version @ hermes_cli/version_info.py:display_version */
void version_display(const char *base_version, const char *derived_version,
                     long distance, int has_distance, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    if (base_version && strcmp(base_version, VERSION_UNKNOWN) == 0) {
        snprintf(out, outsz, "%s", derived_version ? derived_version : "");
        return;
    }
    version_derived(base_version, distance, has_distance, 0, NULL, out, outsz);
}

/* PoP: hermes_user_agent @ hermes_cli/version_info.py:hermes_user_agent */
void version_user_agent(const char *product, const char *base_version,
                        char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    snprintf(out, outsz, "%s/%s", product ? product : "",
             base_version ? base_version : VERSION_UNKNOWN);
}
