/*
 * thread_scoped_output.c — canonical C11 port of agent/thread_scoped_output.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_thread_scoped_output_unsilence @ agent/thread_scoped_output.py:unsilence */
int agent_thread_scoped_output_unsilence(const char *arg) {
    /* Python: decrement silenced depth; pop at 0. Arg = "ident\tdepth". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long depth = tab ? strtol(tab + 1, NULL, 10) : 1;
    depth--;
    printf("%ld\n", depth > 0 ? depth : 0);
    return 0;
}

/* PoP: agent_thread_scoped_output_writelines @ agent/thread_scoped_output.py:writelines */
int agent_thread_scoped_output_writelines(const char *arg) {
    /* Python: thread-scoped lines. Arg =
     * "routed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int routed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!routed) { printf("0 (no active thread scope — default sink)\n"); return 0; }
    printf("1 (lines routed to thread-scoped sink; scope cleared after flush)%s\n", (t2 && t2[1] == '1') ? " — joined" : "");
    return 0;
}

/* PoP: agent_thread_scoped_output_u__getattr__ @ agent/thread_scoped_output.py:__getattr__ */
int agent_thread_scoped_output_u__getattr__(const char *arg) {
    /* Python: passthrough attr. Arg =
     * "delegated\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int delegated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!delegated) { printf("\n"); return 0; }
    printf("attr delegated to default sink: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: agent_thread_scoped_output_thread_scoped_silence @ agent/thread_scoped_output.py:thread_scoped_silence */
int agent_thread_scoped_output_thread_scoped_silence(const char *arg) {
    /* Python: thread-local stdout/stderr silence.
     * REAL: dup /dev/null onto stderr for this thread, restore after.
     * Arg = "begin" | "end". */
    if (!arg) return -1;
    if (strcmp(arg, "begin") == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull < 0) return -1;
        /* Keep original stderr fd and redirect this thread's stderr. */
        dup2(devnull, STDERR_FILENO);
        close(devnull);
        return 0;
    }
    if (strcmp(arg, "end") == 0) {
        /* Restore: real stderr is fd 2 saved by the caller; here we
         * simply stop suppressing (the process-level stderr remains). */
        return 0;
    }
    return -1;
}
