/*
 * gateway_status_paths.c - runtime status/pid/lock/marker file paths.
 *
 * Split from status.c (soul.md: self-contained modules). Path construction
 * and small dir/time helpers; no file-scope state.
 */
#include "gateway_status_paths.h"

#define PID_FILENAME               "gateway.pid"
#define GATEWAY_LOCK_FILENAME      "gateway.lock"
#define RUNTIME_STATUS_FILE        "gateway_state.json"
#define TAKEOVER_MARKER_FILENAME   ".gateway-takeover.json"
#define PLANNED_STOP_MARKER_FILENAME ".gateway-planned-stop.json"
#define LOCKS_DIRNAME              "gateway-locks"
#include "hermes_gateway_core.h"
#include "slermes_home.h"
#include "json.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>   /* flock */
#include <sys/types.h>


/* ── Path builders ───────────────────────────────────────────────────── */

/* Write "<home>/<name>" into buf. Returns buf. */
static char *home_path(char *buf, size_t n, const char *name) {
    snprintf(buf, n, "%s/%s", slermes_home(), name);
    return buf;
}


/* PoP: get_pid_path @ gateway/status.py:_get_pid_path */
char *get_pid_path(char *buf, size_t n) {
    return home_path(buf, n, PID_FILENAME);
}


/* PoP: get_gateway_lock_path @ gateway/status.py:_get_gateway_lock_path */
char *get_gateway_lock_path(const char *pid_path, char *buf, size_t n) {
    if (pid_path && pid_path[0]) {
        /* replace the basename of pid_path with the lock filename */
        const char *slash = strrchr(pid_path, '/');
        if (slash) {
            size_t dirlen = (size_t)(slash - pid_path);
            snprintf(buf, n, "%.*s/%s", (int)dirlen, pid_path, GATEWAY_LOCK_FILENAME);
        } else {
            snprintf(buf, n, "%s", GATEWAY_LOCK_FILENAME);
        }
        return buf;
    }
    return home_path(buf, n, GATEWAY_LOCK_FILENAME);
}


/* PoP: get_runtime_status_path @ gateway/status.py:_get_runtime_status_path */
char *get_runtime_status_path(char *buf, size_t n) {
    return home_path(buf, n, RUNTIME_STATUS_FILE);
}


/* PoP: get_takeover_marker_path @ gateway/status.py:_get_takeover_marker_path */
char *get_takeover_marker_path(char *buf, size_t n) {
    return home_path(buf, n, TAKEOVER_MARKER_FILENAME);
}


/* PoP: get_planned_stop_marker_path @ gateway/status.py:_get_planned_stop_marker_path */
char *get_planned_stop_marker_path(char *buf, size_t n) {
    return home_path(buf, n, PLANNED_STOP_MARKER_FILENAME);
}


/* Machine-local directory for token-scoped gateway locks. Mirrors
 * _get_lock_dir(): HERMES_GATEWAY_LOCK_DIR override, else
 * $XDG_STATE_HOME/hermes/gateway-locks, else ~/.local/state/hermes/... */
char *get_lock_dir(char *buf, size_t n) {
    const char *override = getenv("HERMES_GATEWAY_LOCK_DIR");
    if (override && override[0]) {
        snprintf(buf, n, "%s", override);
        return buf;
    }
    const char *xdg = getenv("XDG_STATE_HOME");
    if (xdg && xdg[0]) {
        snprintf(buf, n, "%s/hermes/%s", xdg, LOCKS_DIRNAME);
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/tmp";
        snprintf(buf, n, "%s/.local/state/hermes/%s", home, LOCKS_DIRNAME);
    }
    return buf;
}


/* mkdir -p on the parent directory of a file path. Best-effort. */
void ensure_parent_dir(const char *file_path) {
    char tmp[1200];
    snprintf(tmp, sizeof(tmp), "%s", file_path);
    char *slash = strrchr(tmp, '/');
    if (!slash) return;
    *slash = '\0';
    /* create intermediate dirs */
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0700); *p = '/'; }
    }
    mkdir(tmp, 0700);
}


/* UTC now as ISO 8601 with +00:00 offset (mirrors datetime.now(utc).isoformat()). */
void utc_now_iso_buf(char *buf, size_t n) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    gmtime_r(&ts.tv_sec, &tmv);
    int micro = (int)(ts.tv_nsec / 1000);
    snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, micro);
}


/* Port of Python: _scope_hash — first 16 hex chars of sha256(identity). */
void scope_hash(const char *identity, char out[17]) {
    char *hex = hash_sha256_hex((const unsigned char *)identity, strlen(identity));
    if (!hex) { out[0] = '\0'; return; }
    memcpy(out, hex, 16);
    out[16] = '\0';
    free(hex);
}


/* ── Scope locks ─────────────────────────────────────────────────────── */

/* Port of Python: _get_scope_lock_path -> "<lockdir>/<scope>-<hash>.lock". */
char *get_scope_lock_path(const char *scope, const char *identity,
                                 char *buf, size_t n) {
    char dir[1100];
    get_lock_dir(dir, sizeof(dir));
    char h[17];
    scope_hash(identity, h);
    snprintf(buf, n, "%s/%s-%s.lock", dir, scope, h);
    return buf;
}
