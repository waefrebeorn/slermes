/*
 * hermes_posix_compat.h — the platform boundary the port has been missing.
 *
 * 52 files call fork() and 63 include <sys/wait.h>. Neither works on Windows: the header
 * exists in mingw, the functions behind it do not. That is the largest single reason 903
 * ported files sit outside build/objects.mk, and no gate can see it, because gates call
 * pure functions and these are process operations.
 *
 * This is the seam, in one place:
 *
 *   hermes_spawn()  -> CreateProcess on Windows, fork+execvp elsewhere.
 *                      Takes argv directly (no shell), so quoting bugs cannot happen.
 *   hermes_wait()   -> WaitForSingleObject + GetExitCodeProcess / waitpid.
 *   hermes_dlopen() -> LoadLibrary / dlopen, with hermes_dlsym / hermes_dlclose.
 *
 * Everything returns -1 / NULL with errno set on failure, matching the POSIX calls, so a
 * caller can be ported by changing the function name and nothing else.
 */
#ifndef HERMES_POSIX_COMPAT_H
#define HERMES_POSIX_COMPAT_H

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
typedef DWORD hermes_pid_t;
#else
#include <sys/types.h>
typedef pid_t hermes_pid_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Result codes for hermes_wait(). */
#define HERMES_WAIT_EXITED   0   /* the child ran and exited; *status holds its code */
#define HERMES_WAIT_TIMEOUT  1   /* still running when the deadline passed */
#define HERMES_WAIT_ERROR   (-1) /* the call itself failed (errno set)            */

/**
 * hermes_spawn — run argv[0] with argv as its arguments (argv[0] is the program).
 *
 * No shell is involved: "cmd && other" is one argument, not two commands. argv must be
 * NULL-terminated. Returns the child's pid, or -1 with errno set.
 */
hermes_pid_t hermes_spawn(char *const argv[]);

/**
 * hermes_wait — wait up to timeout_ms for the child (timeout_ms < 0 waits forever).
 *
 * On HERMES_WAIT_EXITED, *status receives the exit code (the POSIX WEXITSTATUS value).
 * On HERMES_WAIT_TIMEOUT the child is still running and must be waited on again.
 */
int hermes_wait(hermes_pid_t pid, int timeout_ms, int *status);

/**
 * hermes_spawn_in — like hermes_spawn, but runs the child in @p cwd and, when @p silent is
 * non-zero, with stdout/stderr redirected to the platform's null device.
 *
 * The double pattern (chdir + freopen + execvp inside fork) is what most call sites in this
 * tree actually do, so it belongs in the seam rather than being rewritten 52 times.
 */
hermes_pid_t hermes_spawn_in(const char *cwd, char *const argv[], int silent);

/**
 * hermes_spawn_detached — start a child that outlives the caller.
 *
 * Detaches from the session (setsid on POSIX, DETACHED_PROCESS on Windows), redirects stdin
 * from the null device, and sends stdout+stderr to @p log_path in append mode (NULL means
 * the null device). Returns the child pid, or -1 with errno set.
 */
hermes_pid_t hermes_spawn_detached(char *const argv[], const char *log_path);

/** hermes_kill — terminate the child if it is still running. Returns 0 on success. */
int hermes_kill(hermes_pid_t pid);

/** hermes_dlopen / hermes_dlsym / hermes_dlclose — LoadLibrary / dlopen, by name. */
void *hermes_dlopen(const char *path);
void *hermes_dlsym(void *handle, const char *symbol);
int   hermes_dlclose(void *handle);

/** hermes_dlerror — last loader error as text, static storage. */
const char *hermes_dlerror(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_POSIX_COMPAT_H */
