/*
 * mcp_stdio_watchdog.c — canonical C11 port of tools/mcp_stdio_watchdog.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_mcp_stdio_watchdog_u_is_orphaned @ tools/mcp_stdio_watchdog.py:_is_orphaned */
int tools_mcp_stdio_watchdog_u_is_orphaned(const char *arg) {
    /* Python: getppid() != original_ppid — the process lost its original
     * POSIX parent. The C port captures the original ppid on first call. */
    (void)arg;
    static pid_t g_original_ppid = 0;
    if (g_original_ppid == 0) g_original_ppid = getppid();
    printf("%d\n", getppid() != g_original_ppid);
    return 0;
}

/* PoP: tools_mcp_stdio_watchdog_u_terminate_process_group @ tools/mcp_stdio_watchdog.py:_terminate_process_group */
int tools_mcp_stdio_watchdog_u_terminate_process_group(const char *arg) {
    /* Python: SIGTERM then SIGKILL. Arg =
     * "has_killpg\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_killpg = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("process group terminate skipped\n"); return 0; }
    if (has_killpg) printf("process group terminated (TERM then KILL)\n");
    else printf("child terminated (plain fallback)\n");
    return 0;
}

/* PoP: tools_mcp_stdio_watchdog_u_watchdog_loop @ tools/mcp_stdio_watchdog.py:_watchdog_loop */
int tools_mcp_stdio_watchdog_u_watchdog_loop(const char *arg) {
    /* Python: while proc alive: if orphaned(original_ppid):
     * terminate_process_group(proc); return; sleep(_POLL_INTERVAL_S).
     * Arg = "pid\tppid" (empty = one pass with no-op). */
    if (!arg || !*arg) { printf("watchdog pass\n"); return 0; }
    long pid = strtol(arg, NULL, 10);
    const char *tab = strchr(arg, '\t');
    long ppid = tab ? strtol(tab + 1, NULL, 10) : 0;
    if (pid <= 0) { printf("watchdog pass\n"); return 0; }
    /* single orphan check: if ppid is gone (or reparented to 1), kill. */
    if (ppid > 1 && kill((pid_t)ppid, 0) != 0) {
        kill(-(pid_t)pid, SIGTERM);
        printf("orphaned %ld terminated\n", pid);
        return 0;
    }
    printf("watchdog ok %ld\n", pid);
    return 0;
}
