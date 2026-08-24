/*
 * sqlite_runtime.c — canonical C11 port of hermes_cli/sqlite_runtime.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_sqlite_runtime_u_version_tuple @ hermes_cli/sqlite_runtime.py:_version_tuple */
int hermes_cli_sqlite_runtime_u_version_tuple(const char *arg) {
    /* Python: [int(p) for p in parts] padded with zeros to length 3, first 3.
     * Arg = tab-separated version parts. */
    if (!arg || !*arg) { printf("0\t0\t0\n"); return 0; }
    long vals[3] = {0, 0, 0};
    int idx = 0;
    const char *p = arg;
    while (*p && idx < 3) {
        while (*p == '\t') p++;
        if (!*p) break;
        vals[idx++] = strtol(p, (char **)&p, 10);
    }
    printf("%ld\t%ld\t%ld\n", vals[0], vals[1], vals[2]);
    return 0;
}

/* PoP: hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:is_sqlite_wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(const char *arg) {
    /* Python: version-range WAL-reset bug check. Arg = "x.y.z". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long v[3] = {0, 0, 0};
    int idx = 0;
    const char *p = arg;
    while (*p && idx < 3) {
        if (*p == '.') { idx++; p++; continue; }
        if (*p >= '0' && *p <= '9') v[idx] = v[idx] * 10 + (*p - '0');
        else break;
        p++;
    }
    if (v[0] < 3 || (v[0] == 3 && v[1] < 7)) { printf("0\n"); return 0; }
    if (v[0] > 3 || v[1] > 51 || (v[1] == 51 && v[2] >= 3)) { printf("0\n"); return 0; }
    if (v[1] == 50 && v[2] >= 7) { printf("0\n"); return 0; }
    if (v[1] == 44 && v[2] >= 6) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_sqlite_runtime_wal_reset_vulnerable @ hermes_cli/sqlite_runtime.py:wal_reset_vulnerable */
int hermes_cli_sqlite_runtime_wal_reset_vulnerable(const char *arg) {
    /* Python: version tuple check for SQLite's WAL-reset bug. Arg =
     * "major.minor.patch". */
    if (!arg || !*arg) return 0;
    int maj = 0, min = 0, pat = 0;
    sscanf(arg, "%d.%d.%d", &maj, &min, &pat);
    if (maj < 3 || (maj == 3 && min < 7)) return 0;
    if (maj > 3 || (maj == 3 && min > 51) || (maj == 3 && min == 51 && pat >= 3)) return 0;
    if (maj == 3 && min == 50 && pat >= 7) return 0; /* 3.50.7 <= v < 3.51.0 */
    if (maj == 3 && min == 44 && pat >= 6) return 0; /* 3.44.6 <= v < 3.45.0 */
    return 1;
}

/* PoP: hermes_cli_sqlite_runtime_probe_sqlite_runtime @ hermes_cli/sqlite_runtime.py:probe_sqlite_runtime */
int hermes_cli_sqlite_runtime_probe_sqlite_runtime(const char *arg) {
    /* Python: isolated child probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}
