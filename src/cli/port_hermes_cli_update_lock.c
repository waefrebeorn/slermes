/*
 * port_hermes_cli_update_lock.c — C11 port of hermes_cli/update_lock.py
 *
 * Cross-process mutual exclusion for in-flight Hermes updates.
 * Uses a marker file <HERMES_HOME>/.hermes-update-in-progress
 * containing "<pid>\n<started_at_unix>\n".
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "port_hermes_cli_update_lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include "libjson/json.h"

/* PoP: update_marker_path @ hermes_cli/update_lock.py:update_marker_path */
char *ul_update_marker_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/.hermes-update-in-progress", home);
    return path;
}

/* PoP: _pid_alive @ hermes_cli/update_lock.py:_pid_alive */
bool ul_pid_alive(int pid) {
    if (pid <= 0) return false;
    /* Delegate to gateway.status._pid_exists equivalent:
     * kill(pid, 0) is NOT safe on Windows, but slermes is Unix-only. */
    return kill(pid, 0) == 0;
}

/* PoP: _handoff_pid @ hermes_cli/update_lock.py:_handoff_pid */
int ul_handoff_pid(void) {
    const char *raw = getenv("HERMES_UPDATE_HANDOFF_PID");
    if (!raw || !*raw) return -1;
    /* strip whitespace */
    const char *s = raw;
    while (*s && (*s == ' ' || *s == '\t')) s++;
    if (!*s) return -1;
    char *end = NULL;
    long pid = strtol(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    return pid > 0 ? (int)pid : -1;
}

/* PoP: _is_ancestor_pid @ hermes_cli/update_lock.py:_is_ancestor_pid */
bool ul_is_ancestor_pid(int pid) {
    if (pid <= 0) return false;
    /* Walk /proc/self/status parent chain on Linux.
     * Not ancestor (parent chain) check — use /proc/<pid>/stat PPid. */
    pid_t cur = getpid();
    /* Walk up the parent chain via /proc */
    for (int i = 0; i < 256; i++) {
        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", (int)cur);
        FILE *f = fopen(stat_path, "r");
        if (!f) break;
        char line[1024];
        if (!fgets(line, sizeof(line), f)) { fclose(f); break; }
        fclose(f);
        /* Format: pid (comm) state ppid ... */
        int ppid = 0;
        /* Find last ')' to skip comm field which may contain spaces/parens */
        char *rparen = strrchr(line, ')');
        if (!rparen) break;
        char *tok = rparen + 1;
        int state;
        if (sscanf(tok, "%d %d", &state, &ppid) != 2) break;
        if (ppid == pid) return true;
        if (ppid == 0 || ppid == 1) break; /* reached init */
        if (ppid == (int)getpid()) break; /* our own pid — loop protection */
        cur = (pid_t)ppid;
    }
    return false;
}

/* PoP: read_live_update @ hermes_cli/update_lock.py:read_live_update */
UpdateHolder *ul_read_live_update(const char *path) {
    if (!path) path = ul_update_marker_path();
    if (!path) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char buf[256];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nread] = '\0';

    if (nread == 0) return NULL;

    /* Parse two lines: pid and started_at */
    char *nl = strchr(buf, '\n');
    if (!nl) return NULL;
    *nl = '\0';
    char *end = NULL;
    long pid = strtol(buf, &end, 10);
    if (end == buf) pid = -1;

    char *start_str = nl + 1;
    double started_at = strtod(start_str, &end);
    if (end == start_str) started_at = -1.0;

    double age = difftime(time(NULL), (time_t)started_at);
    if (!ul_pid_alive((int)pid) || age > UPDATE_MARKER_MAX_AGE_SECONDS) {
        /* Stale marker — remove it */
        unlink(path);
        return NULL;
    }

    UpdateHolder *h = malloc(sizeof(UpdateHolder));
    if (!h) return NULL;
    h->pid = (int)pid;
    h->age_seconds = age;
    return h;
}

/* PoP: describe_holder @ hermes_cli/update_lock.py:describe_holder */
char *ul_describe_holder(const UpdateHolder *holder) {
    if (!holder) return NULL;
    int total = (int)holder->age_seconds;
    int minutes = total / 60;
    int seconds = total % 60;
    char elapsed[32];
    if (minutes) snprintf(elapsed, sizeof(elapsed), "%dm %ds", minutes, seconds);
    else snprintf(elapsed, sizeof(elapsed), "%ds", seconds);

    char *out = NULL;
    asprintf(&out,
        "✗ Another Hermes update is already running (PID %d, started %s ago).\n"
        "\n"
        "  Two updates mutating the same checkout corrupt it: one rewrites\n"
        "  source while the other is mid-install. Wait for it to finish, or\n"
        "  close the window/dashboard tab that started it, then retry.",
        holder->pid, elapsed);
    return out;
}

/* PoP: __init__ @ hermes_cli/update_lock.py:UpdateLock.__init__ */
UpdateLock *ul_update_lock_new(const char *path) {
    UpdateLock *lock = malloc(sizeof(UpdateLock));
    if (!lock) return NULL;
    lock->path = path ? strdup(path) : ul_update_marker_path();
    lock->acquired = false;
    lock->holder = NULL;
    return lock;
}

/* PoP: acquire @ hermes_cli/update_lock.py:UpdateLock.acquire */
bool ul_update_lock_acquire(UpdateLock *lock) {
    if (!lock) return false;
    UpdateHolder *existing = ul_read_live_update(lock->path);
    if (existing) {
        if (existing->pid == ul_handoff_pid() || ul_is_ancestor_pid(existing->pid)) {
            free(existing);
            return true; /* running under our parent's claim */
        }
        lock->holder = existing;
        return false;
    }
    /* Try to write the marker */
    char *parent = strdup(lock->path);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        mkdir(parent, 0755);
    }
    free(parent);

    FILE *f = fopen(lock->path, "w");
    if (!f) {
        free(existing); /* existing is NULL here */
        return true; /* best-effort: degrade to pre-lock behavior */
    }
    fprintf(f, "%d\n%ld\n", (int)getpid(), (long)time(NULL));
    fclose(f);

    lock->acquired = true;
    return true;
}

/* PoP: release @ hermes_cli/update_lock.py:UpdateLock.release */
void ul_update_lock_release(UpdateLock *lock) {
    if (!lock || !lock->acquired) return;
    lock->acquired = false;

    FILE *f = fopen(lock->path, "r");
    if (!f) return;
    char buf[256];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nread] = '\0';

    char *nl = strchr(buf, '\n');
    if (!nl) return;
    *nl = '\0';
    char *end = NULL;
    long pid = strtol(buf, &end, 10);
    if (end == buf) return;
    if ((int)pid != (int)getpid()) return; /* handoff partner took ownership */

    unlink(lock->path);
}

/* PoP: __enter__ @ hermes_cli/update_lock.py:UpdateLock.__enter__ */
bool ul_update_lock_enter(UpdateLock *lock) {
    return ul_update_lock_acquire(lock);
}

/* PoP: __exit__ @ hermes_cli/update_lock.py:UpdateLock.__exit__ */
void ul_update_lock_exit(UpdateLock *lock) {
    ul_update_lock_release(lock);
}

void ul_update_lock_free(UpdateLock *lock) {
    if (!lock) return;
    free(lock->path);
    if (lock->holder) {
        free(lock->holder);
    }
    free(lock);
}

void ul_update_holder_free(UpdateHolder *holder) {
    free(holder);
}
