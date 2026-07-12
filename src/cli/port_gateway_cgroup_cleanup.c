/*
 * port_gateway_cgroup_cleanup.c — C port of gateway/cgroup_cleanup.py
 *
 * Pure systemd-ExecStopPost helper: SIGKILL every PID (except self) in this
 * process's cgroup. No config, no network. Faithful to _own_cgroup_path,
 * _read_cgroup_pids, reap_cgroup.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

/* PoP: cgroup_own_path @ gateway/cgroup_cleanup.py:_own_cgroup_path */
/* Return malloc'd cgroup v2 path for the calling process, or NULL. */
char *cgroup_own_path(void) {
    FILE *f = fopen("/proc/self/cgroup", "r");
    if (!f) return NULL;
    char *line = NULL; size_t cap = 0; ssize_t n;
    char *result = NULL;
    while ((n = getline(&line, &cap, f)) != -1) {
        /* match "^0::(...)$" */
        if (strncmp(line, "0::", 3) == 0) {
            char *p = line + 3;
            /* strip trailing newline */
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            result = strdup(p);
            break;
        }
    }
    free(line);
    fclose(f);
    return result;
}

/* PoP: cgroup_read_pids @ gateway/cgroup_cleanup.py:_read_cgroup_pids */
/* Read PIDs from /sys/fs/cgroup<path>/cgroup.procs into a malloc'd int[].
 * *out_count receives the count. Returns NULL on failure. */
int *cgroup_read_pids(const char *cgroup_path, int *out_count) {
    *out_count = 0;
    if (!cgroup_path) return NULL;
    char path[4096];
    snprintf(path, sizeof(path), "/sys/fs/cgroup%s/cgroup.procs", cgroup_path);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    int *pids = NULL; int cap = 0, cnt = 0;
    char lbuf[64];
    while (fgets(lbuf, sizeof(lbuf), f)) {
        int pid;
        if (sscanf(lbuf, "%d", &pid) != 1) continue;
        if (cnt == cap) {
            cap = cap ? cap * 2 : 16;
            int *np = realloc(pids, cap * sizeof(int));
            if (!np) break;
            pids = np;
        }
        pids[cnt++] = pid;
    }
    fclose(f);
    *out_count = cnt;
    return pids;
}

/* PoP: cgroup_reap @ gateway/cgroup_cleanup.py:reap_cgroup */
/* SIGKILL every PID in the cgroup other than the caller. Returns count killed. */
int cgroup_reap(const char *cgroup_path) {
    char *own = NULL;
    if (!cgroup_path) {
        own = cgroup_own_path();
        if (!own) return 0;
        cgroup_path = own;
    }
    int n = 0;
    int *pids = cgroup_read_pids(cgroup_path, &n);
    free(own);
    if (!pids) return 0;
    int own_pid = getpid();
    int killed = 0;
    for (int i = 0; i < n; i++) {
        if (pids[i] == own_pid) continue;
        if (kill(pids[i], SIGKILL) == 0) killed++;
    }
    free(pids);
    return killed;
}
