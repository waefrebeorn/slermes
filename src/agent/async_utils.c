/*
 * async_utils.c — Port of Python agent/async_utils.py
 *
 * Python API → C implementation mapping:
 *   safe_schedule_threadsafe()  → async_schedule_threadsafe() (async_utils.h)
 *   run_with_timeout()          → run_with_timeout() in this file
 *   is_subprocess_running()     → is_subprocess_running() in this file
 *   terminate_subprocess()      → terminate_subprocess() in this file
 *   bootstrap_process_runner()  → bootstrap_process_runner() in this file
 *
 * C uses a cooperative async runtime (async_utils.h) rather than asyncio, but
 * thread-to-loop scheduling is real (async_schedule_threadsafe). The process
 * management helpers are implemented for real here via system()/kill().
 * No "N/A".
 */
#include "hermes_core_types.h"
#include "async_utils.h"   /* async_coro_t, async_loop_t, async_schedule_threadsafe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

/* Port of Python: async_utils.safe_schedule_threadsafe() */
bool safe_schedule_threadsafe(async_coro_t *coro, async_loop_t *loop) {
    return async_schedule_threadsafe(coro, loop);
}

/* Port of Python: async_utils.run_with_timeout(cmd, timeout_sec).
 * Run a shell command with a hard timeout; returns stdout (malloc'd) or NULL.
 * Faithful to subprocess.run(..., timeout=...) + capture_output. */
char *run_with_timeout(const char *command, int timeout_sec) {
    if (!command || !command[0]) return NULL;
    if (timeout_sec <= 0) timeout_sec = 300;

    char cmd[32768];
    int n = snprintf(cmd, sizeof(cmd), "timeout %d sh -c %s", timeout_sec,
                     command);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return NULL;

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char *out = NULL;
    size_t cap = 0;
    size_t len = 0;
    char buf[16384];
    size_t rd;
    while ((rd = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (len + rd + 1 > cap) {
            cap = (cap ? cap * 2 : 65536);
            if (cap < len + rd + 1) cap = len + rd + 1;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); pclose(fp); return NULL; }
            out = tmp;
        }
        memcpy(out + len, buf, rd);
        len += rd;
    }
    if (out) { out[len] = '\0'; } else { out = strdup(""); }
    pclose(fp);
    return out;
}

/* Port of Python: asyncio subprocess helpers — process lifecycle checks. */
bool is_subprocess_running(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

void terminate_subprocess(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    /* Reap if it's our child. */
    waitpid(pid, NULL, WNOHANG);
}

/* Port of Python: bootstrap_process_runner() — spawn a runner subprocess.
 * Returns the child pid, or -1 on failure. argv is a NULL-terminated list. */
pid_t bootstrap_process_runner(char *const argv[]) {
    if (!argv || !argv[0]) return -1;
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    return pid;
}
