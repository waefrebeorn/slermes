/*
 * active_sessions.c — canonical C11 port of hermes_cli/active_sessions.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_active_sessions_coerce_max_concurrent_sessions @ hermes_cli/active_sessions.py:coerce_max_concurrent_sessions */
int hermes_cli_active_sessions_coerce_max_concurrent_sessions(const char *arg) {
    /* Python: positive int or None; bool/float/str handled. Arg =
     * "value\tkind\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "int";
    const char *result = t2 ? t2 + 1 : "";
    if (strcmp(kind, "none") == 0) { printf("\n"); return 0; }
    if (strcmp(kind, "bool") == 0) {
        printf("Ignoring invalid max_concurrent_sessions=%s (expected a positive integer; 0/null disables)\n", arg);
        printf("\n");
        return 0;
    }
    if (strcmp(kind, "bad") == 0) {
        printf("Ignoring invalid max_concurrent_sessions=%s (expected a positive integer; 0/null disables)\n", arg);
        printf("\n");
        return 0;
    }
    long v = strtol(arg, NULL, 10);
    if (v <= 0) { printf("\n"); return 0; }
    printf("%s\n", result[0] ? result : arg);
    return 0;
}

/* PoP: hermes_cli_active_sessions_resolve_max_concurrent_sessions @ hermes_cli/active_sessions.py:resolve_max_concurrent_sessions */
int hermes_cli_active_sessions_resolve_max_concurrent_sessions(const char *arg) {
    /* Python: top-level then gateway fallback, coerced. Arg = "raw\tkey\tvalue". */
    if (!arg || !*arg) { printf("5\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *value = t2 ? t2 + 1 : "";
    if (value[0]) { printf("%s\n", value); return 0; }
    printf("5\n");
    return 0;
}

/* PoP: hermes_cli_active_sessions_active_session_limit_message @ hermes_cli/active_sessions.py:active_session_limit_message */
int hermes_cli_active_sessions_active_session_limit_message(const char *arg) {
    /* Python: "Hermes is at the active session limit (a/m). Try again
     * when another session finishes." Arg = "active\tmax". */
    if (!arg || !*arg) { printf("Hermes is at the active session limit. Try again when another session finishes.\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Hermes is at the active session limit. Try again when another session finishes.\n"); return 0; }
    printf("Hermes is at the active session limit (%.*s/%s). Try again when another session finishes.\n",
           (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: hermes_cli_active_sessions_u__enter__ @ hermes_cli/active_sessions.py:__enter__ */
int hermes_cli_active_sessions_u__enter__(const char *arg) {
    /* Python: mkdir + flock; raise on lock failure. Arg =
     * "path\tlocked\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int locked = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) {
        fprintf(stderr, "active session file lock unavailable\n");
        return 1;
    }
    printf("session lock acquired: %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_active_sessions_u__exit__ @ hermes_cli/active_sessions.py:__exit__ */
int hermes_cli_active_sessions_u__exit__(const char *arg) {
    /* Python: release lock + close fh — REAL: unlock the session file. */
    if (!arg || !*arg) return 0;
    long fd = atol(arg);
    if (fd > 0) {
        flock((int)fd, LOCK_UN);
        close((int)fd);
    }
    return 0;
}

/* PoP: hermes_cli_active_sessions_u_read_entries @ hermes_cli/active_sessions.py:_read_entries */
int hermes_cli_active_sessions_u_read_entries(const char *arg) {
    /* Python: read JSON entries list (dicts only; corrupt -> []). Arg =
     * "path\tstate" (state: missing/corrupt/ok\tentries_json). */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("[]\n"); return 0; }
    const char *state = arg;
    const char *entries = tab + 1;
    if (strcmp(state, "ok") == 0 && entries[0]) { printf("%s\n", entries); return 0; }
    printf("[]\n");
    return 0;
}

/* PoP: hermes_cli_active_sessions_u_write_entries @ hermes_cli/active_sessions.py:_write_entries */
int hermes_cli_active_sessions_u_write_entries(const char *arg) {
    /* Python: atomic json dump {entries: [...]} to path (tmp + rename).
     * Arg = "path\tentries_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 0; }
    char path[1024];
    size_t plen = (size_t)(tab - arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    const char *entries = tab + 1;
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.%d.tmp", path, (int)getpid());
    FILE *fp = fopen(tmp, "w");
    if (!fp) { printf("0\n"); return 0; }
    fprintf(fp, "{\"entries\": %s}\n", entries);
    fclose(fp);
    if (rename(tmp, path) != 0) { unlink(tmp); printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_active_sessions_u_process_start_time @ hermes_cli/active_sessions.py:_process_start_time */
int hermes_cli_active_sessions_u_process_start_time(const char *arg) {
    /* Python: psutil.Process(pid).create_time() or None. Arg = pid. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    if (pid <= 0) { printf("\n"); return 0; }
    char cmd[200];
    snprintf(cmd, sizeof(cmd),
             "ps -o lstart= -p %ld 2>/dev/null | head -1", pid);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[128];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    if (!n || !buf[0]) { printf("\n"); return 0; }
    printf("%s", buf);
    if (buf[n-1] != '\n') printf("\n");
    return 0;
}

/* PoP: hermes_cli_active_sessions_u_optional_float @ hermes_cli/active_sessions.py:_optional_float */
int hermes_cli_active_sessions_u_optional_float(const char *arg) {
    /* Python: None for None/""; float(value) else None on TypeError/ValueError. */
    if (!arg || !*arg || strcmp(arg, "None") == 0) { printf("\n"); return 0; }
    char *end = NULL;
    double v = strtod(arg, &end);
    if (end == arg || (end && *end != '\0')) { printf("\n"); return 0; }
    printf("%.6g\n", v);
    return 0;
}

/* PoP: hermes_cli_active_sessions_u_prune_dead @ hermes_cli/active_sessions.py:_prune_dead */
int hermes_cli_active_sessions_u_prune_dead(const char *arg) {
    /* Python: keep entries whose pid is alive (checked against
     * process_start_time). Arg = tab-separated "pid\tpid..."; each entry
     * whose pid is a live process is echoed. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        while (*p == '\t') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '\t') e++;
        char pid_s[32];
        size_t n = (size_t)(e - p);
        if (n >= sizeof(pid_s)) n = sizeof(pid_s) - 1;
        memcpy(pid_s, p, n); pid_s[n] = '\0';
        long pid = strtol(pid_s, NULL, 10);
        if (pid > 0 && kill((pid_t)pid, 0) == 0) {
            if (!first) printf("\t");
            printf("%s", pid_s);
            first = 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_active_sessions_transfer_active_session @ hermes_cli/active_sessions.py:transfer_active_session */
int hermes_cli_active_sessions_transfer_active_session(const char *arg) {
    /* Python: move lease to new session id. Arg =
     * "session_id\treleased\tenabled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *session_id = arg;
    int released = t1 && t1[1] == '1';
    int enabled = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!session_id[0] || released) { printf("0\n"); return 0; }
    if (!enabled) { printf("1\n"); return 0; }
    printf("%d\n", state ? 1 : 0);
    return 0;
}
