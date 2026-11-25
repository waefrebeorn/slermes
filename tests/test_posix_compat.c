/*
 * test_posix_compat.c — prove the platform seam actually works, not just that it compiles.
 *
 * Two properties are checked, because they are the two the port depends on:
 *   1. hermes_spawn + hermes_wait return the child's real exit code.
 *   2. hermes_dlopen + hermes_dlsym resolve a real symbol from a real library.
 *
 * Build: gcc -O2 -Iinclude -o .testbin/test_posix_compat.exe \
 *            tests/test_posix_compat.c src/hermes_posix_compat.c
 * Exit code 0 means both passed.
 */
#include "hermes_posix_compat.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    int failures = 0;

    /* ── 1. spawn + wait ──────────────────────────────────────────────────── */
#ifdef _WIN32
    char *argv[] = { (char *)"cmd", (char *)"/c", (char *)"exit 7", NULL };
#else
    char *argv[] = { (char *)"sh", (char *)"-c", (char *)"exit 7", NULL };
#endif
    hermes_pid_t pid = hermes_spawn(argv);
    if (pid == (hermes_pid_t)-1) {
        printf("FAIL spawn: errno=%d\n", errno);
        failures++;
    } else {
        int status = -1;
        int rc = hermes_wait(pid, 30000, &status);
        if (rc != HERMES_WAIT_EXITED) {
            printf("FAIL wait: rc=%d (expected HERMES_WAIT_EXITED)\n", rc);
            hermes_kill(pid);
            failures++;
        } else if (status != 7) {
            printf("FAIL exit status: %d (expected 7)\n", status);
            failures++;
        } else {
            printf("ok   spawn+wait: exit code 7 observed\n");
        }
    }

    /* ── 2. dynamic loading ───────────────────────────────────────────────── */
#ifdef _WIN32
    const char *lib = "kernel32.dll";
    const char *sym = "GetTickCount";
#else
    const char *lib = "libc.so.6";
    const char *sym = "strlen";
#endif
    void *h = hermes_dlopen(lib);
    if (!h) {
        printf("FAIL dlopen(%s): %s\n", lib, hermes_dlerror());
        failures++;
    } else {
        void *p = hermes_dlsym(h, sym);
        if (!p) {
            printf("FAIL dlsym(%s): %s\n", sym, hermes_dlerror());
            failures++;
        } else {
            printf("ok   dlopen+dlsym: %s!%s at %p\n", lib, sym, p);
        }
        hermes_dlclose(h);
    }

    printf(failures ? "posix_compat: FAIL\n" : "posix_compat: PASS\n");
    return failures ? 1 : 0;
}
