/* Slermes C port — gateway/cgroup_cleanup.py (cgroup process reaper) */

#include <stdbool.h>
#include "hermes_gateway_core.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

/* PoP: gateway_cgroup_cleanup_own_cgroup_path @ gateway/cgroup_cleanup.py:_own_cgroup_path */
int gateway_cgroup_cleanup_own_cgroup_path(const char *buf, char *out, size_t outsz)
{
    out[0] = '\0';
    if (!buf || !*buf) return 0;
    regex_t re;
    if (regcomp(&re, "^0::(.+)$", REG_EXTENDED | REG_NEWLINE) != 0) return 0;
    regmatch_t m[2];
    const char *p = buf;
    int rc;
    while ((rc = regexec(&re, p, 1, m, 0)) == 0) {
        const char *line = p + m[0].rm_so;
        const char *colon = strstr(line, "0::");
        if (colon) {
            const char *val = colon + 3;
            const char *nl = strpbrk(val, "\n\r");
            size_t len = nl ? (size_t)(nl - val) : strlen(val);
            while (len > 0 && (val[len-1] == ' ' || val[len-1] == '\t')) len--;
            if (len > 0 && len < outsz) { memcpy(out, val, len); out[len] = '\0'; }
            regfree(&re);
            return 1;
        }
        p += m[0].rm_eo;
        if (*p == '\0') break;
    }
    regfree(&re);
    return 0;
}

/* PoP: _read_cgroup_pids @ gateway/cgroup_cleanup.py:_read_cgroup_pids */
/* Read /sys/fs/cgroup<path>/cgroup.procs, parse each line as a PID into out[]
 * (max cap). Returns the number of PIDs written (0 on missing/unreadable). */
size_t gateway_cgroup_cleanup_read_pids(const char *cgroup_path, int *out, size_t cap)
{
    if (!cgroup_path || !*cgroup_path || !out || !cap) return 0;
    char path[1024];
    int n = snprintf(path, sizeof(path), "/sys/fs/cgroup%s/cgroup.procs", cgroup_path);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t count = 0;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char *end;
        long pid = strtol(line, &end, 10);
        if (end == line || *end != '\n') continue;   /* not a clean integer line */
        if (count < cap) out[count] = (int)pid;
        count++;
    }
    fclose(f);
    return count;
}

/* PoP: reap_cgroup @ gateway/cgroup_cleanup.py:reap_cgroup */
/* SIGKILL every PID in the cgroup other than the caller. Returns the count
 * killed. cgroup_path may be NULL (resolved from /proc/self/cgroup). */
int gateway_cgroup_cleanup_reap(const char *cgroup_path)
{
    char resolved[1024];
    const char *path = cgroup_path;
    if (!path || !*path) {
        FILE *f = fopen("/proc/self/cgroup", "r");
        if (!f) return 0;
        char buf[4096];
        size_t total = 0;
        while (fgets(buf + total, sizeof(buf) - total, f)) total += strlen(buf + total);
        fclose(f);
        if (!gateway_cgroup_cleanup_own_cgroup_path(buf, resolved, sizeof(resolved))) return 0;
        path = resolved;
    }
    if (!*path) return 0;
    int pids[4096];
    size_t n = gateway_cgroup_cleanup_read_pids(path, pids, 4096);
    int own = (int)getpid();
    int killed = 0;
    for (size_t i = 0; i < n; i++) {
        if (pids[i] == own) continue;
        if (kill((pid_t)pids[i], SIGKILL) == 0) killed++;
    }
    return killed;
}
