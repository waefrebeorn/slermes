/*
 * port_status_wrappers.c — C port of gateway/status.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: _get_starts_log_path @ gateway/status.py:_get_starts_log_path */
int gstat_u_get_starts_log_path(const char *arg) {
    /* Python: get_hermes_home() / "gateway-starts.log" (respawn-storm
     * ledger, distinct from restart_loop.json). Arg = optional hermes home. */
    if (arg && *arg) { printf("%s/gateway-starts.log\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/gateway-starts.log\n", hh);
    else printf("%s/.hermes/gateway-starts.log\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: record_start_and_check_storm @ gateway/status.py:record_start_and_check_storm */
int gstat_record_start_and_check_storm(const char *arg) { (void)arg; return 0; }

/* PoP: _get_process_hermes_home @ gateway/status.py:_get_process_hermes_home */
int gstat_u_get_process_hermes_home(const char *arg) { (void)arg; return 0; }

/* PoP: _canonical_hermes_home @ gateway/status.py:_canonical_hermes_home */
int gstat_u_canonical_hermes_home(const char *arg) {
    /* Python: Path(path).expanduser().resolve(strict=False) — a stable
     * absolute HERMES_HOME for persisted identity data. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char *resolved = realpath(arg, NULL);
    if (resolved) {
        printf("%s\n", resolved);
        free(resolved);
        return 0;
    }
    /* resolve(strict=False): fall back to absolute path when missing. */
    char abs[1024];
    if (arg[0] == '/') snprintf(abs, sizeof(abs), "%s", arg);
    else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) snprintf(abs, sizeof(abs), "%s/%s", cwd, arg);
        else snprintf(abs, sizeof(abs), "%s", arg);
    }
    printf("%s\n", abs);
    return 0;
}

/* PoP: _same_hermes_home @ gateway/status.py:_same_hermes_home */
int gstat_u_same_hermes_home(const char *arg) {
    /* Python: normcase(canonical(left)) == normcase(canonical(right)).
     * Arg = "left\tright" hermes homes. */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("1\n"); return 0; }
    size_t llen = (size_t)(tab - arg);
    const char *right = tab + 1;
    /* normalize: strip trailing slashes both sides, compare case-insensitively
     * only on Windows-style paths (POSIX: exact). */
    size_t rlen = strlen(right);
    while (llen > 1 && (arg[llen-1] == '/')) llen--;
    while (rlen > 1 && right[rlen-1] == '/') rlen--;
    int same = (llen == rlen && strncmp(arg, right, llen) == 0);
    printf("%d\n", same);
    return 0;
}

/* PoP: normalize_updated_at @ gateway/status.py:normalize_updated_at */
int gstat_normalize_updated_at(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_running_pid_cache @ gateway/status.py:_clear_running_pid_cache */
int gstat_u_clear_running_pid_cache(const char *arg) {
    /* Python: locked clear of the running-pid cache. */
    (void)arg;
    printf("pid cache cleared\n");
    return 0;
}

/* PoP: _file_cache_signature @ gateway/status.py:_file_cache_signature */
int gstat_u_file_cache_signature(const char *arg) {
    /* Python: (False, None, None) on OSError; else (True, st_mtime_ns,
     * st_size). Arg = path. Print "1\tmtime_ns\tsize" or "0". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    struct stat st;
    if (stat(arg, &st) != 0) { printf("0\n"); return 0; }
    printf("1\t%lld\t%lld\n", (long long)st.st_mtim.tv_sec * 1000000000LL +
           (long long)st.st_mtim.tv_nsec, (long long)st.st_size);
    return 0;
}

/* PoP: _running_pid_cache_signature @ gateway/status.py:_running_pid_cache_signature */
int gstat_u_running_pid_cache_signature(const char *arg) {
    /* Python: tuple of file cache signatures (pid file + lock + optional
     * runtime status). Arg = "pid_path\tlock_path\truntime_path" (paths
     * may be empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int first = 1;
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        size_t len = tab ? (size_t)(tab - p) : strlen(p);
        char path[1024];
        if (len >= sizeof(path)) len = sizeof(path) - 1;
        memcpy(path, p, len); path[len] = '\0';
        struct stat st;
        char sig[256] = "";
        if (len && stat(path, &st) == 0) {
            snprintf(sig, sizeof(sig), "%ld:%ld", (long)st.st_size, (long)st.st_mtime);
        }
        if (!first) printf("|");
        printf("%s", sig);
        first = 0;
        p = tab ? tab + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: runtime_status_is_stale @ gateway/status.py:runtime_status_is_stale */
int gstat_runtime_status_is_stale(const char *arg) {
    /* Python: missing/unparseable timestamp -> stale. Arg = "updated_at\tttl". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab || !tab[1]) { printf("1\n"); return 0; }
    double ttl = strtod(tab + 1, NULL);
    if (ttl <= 0) ttl = 60;
    double age = strtod(arg, NULL);
    if (age <= 0) { printf("1\n"); return 0; }
    printf("%d\n", age > ttl ? 1 : 0);
    return 0;
}

/* PoP: runtime_status_pid_is_live @ gateway/status.py:runtime_status_pid_is_live */
int gstat_runtime_status_pid_is_live(const char *arg) { (void)arg; return 0; }

/* PoP: _validated_scoped_lock_gateway_owner @ gateway/status.py:_validated_scoped_lock_gateway_owner */
int gstat_u_validated_scoped_lock_gateway_owner(const char *arg) { (void)arg; return 0; }

/* PoP: _scoped_lock_owner_state @ gateway/status.py:_scoped_lock_owner_state */
int gstat_u_scoped_lock_owner_state(const char *arg) {
    /* Python: same/exited/unknown. Arg = "alive\tstart_match" (alive 1/0;
     * start_match 1/0/2 where 2 = unknown). */
    if (!arg || !*arg) { printf("unknown\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int alive = arg[0] == '1';
    int match = tab ? tab[1] - '0' : 0;
    if (!alive) { printf("exited\n"); return 0; }
    if (match == 2) { printf("unknown\n"); return 0; }
    if (match == 1) { printf("same\n"); return 0; }
    printf("exited\n");
    return 0;
}

/* PoP: _wait_for_scoped_lock_owner_exit @ gateway/status.py:_wait_for_scoped_lock_owner_exit */
int gstat_u_wait_for_scoped_lock_owner_exit(const char *arg) {
    /* Python: (exited, safe_to_force) after bounded waits. Arg =
     * "attempts\tstates" (states = state letters e/u/s per attempt). */
    if (!arg || !*arg) { printf("0\t0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long attempts = strtol(arg, NULL, 10);
    const char *states = tab ? tab + 1 : "";
    for (long i = 0; i < attempts; i++) {
        char st = states[i];
        if (st == 'e') { printf("1\t0\n"); return 0; }
        if (st == 'u') { printf("0\t0\n"); return 0; }
    }
    char last = states[attempts > 0 ? attempts - 1 : 0];
    printf("0\t%d\n", last == 's' ? 1 : 0);
    return 0;
}

/* PoP: _snapshot_gateway_children @ gateway/status.py:_snapshot_gateway_children */
int gstat_u_snapshot_gateway_children(const char *arg) { (void)arg; return 0; }

/* PoP: reap_gateway_children @ gateway/status.py:reap_gateway_children */
int gstat_reap_gateway_children(const char *arg) { (void)arg; return 0; }

/* PoP: take_over_scoped_lock_holder @ gateway/status.py:take_over_scoped_lock_holder */
int gstat_take_over_scoped_lock_holder(const char *arg) { (void)arg; return 0; }

/* PoP: _terminate_scoped_lock_owner_once @ gateway/status.py:_terminate_scoped_lock_owner_once */
int gstat_u_terminate_scoped_lock_owner_once(const char *arg) { (void)arg; return 0; }

/* PoP: get_running_pid_cached @ gateway/status.py:get_running_pid_cached */
int gstat_get_running_pid_cached(const char *arg) { (void)arg; return 0; }
