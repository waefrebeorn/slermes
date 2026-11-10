/* Slermes Windows compat: <sys/wait.h> — no fork() on Windows.
 * These are compile-time stubs so CLI paths link; process-spawning code is a
 * known behavioural gap, not a silent one. */
#ifndef SLERMES_COMPAT_SYS_WAIT_H
#define SLERMES_COMPAT_SYS_WAIT_H

#include <sys/types.h>

#ifndef WNOHANG
#define WNOHANG 1
#endif

#define WIFEXITED(s)    (1)
#define WEXITSTATUS(s)  ((s) & 0xff)
#define WIFSIGNALED(s)  (0)
#define WTERMSIG(s)     (0)
#define WIFSTOPPED(s)   (0)

/* No process table to reap — report "nothing to wait for". */
static inline pid_t waitpid(pid_t pid, int *status, int options)
{
    (void)pid; (void)options;
    if (status) *status = 0;
    return -1;
}

#endif /* SLERMES_COMPAT_SYS_WAIT_H */
