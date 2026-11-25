/*
 * hermes_posix_compat.c — CreateProcess / LoadLibrary implementations of the platform seam.
 *
 * See hermes_posix_compat.h for why this exists. The Windows side is the real one here:
 * this port is built and run on Windows, and every fork()/dlopen() call in the tree is a
 * link error waiting to happen.
 */
#include "hermes_posix_compat.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

/* ── spawn / wait / kill ───────────────────────────────────────────────────── */

/* Quote one argument for the CreateProcess command line. Windows parses the line, not an
 * argv array, so an argument with spaces or quotes has to be escaped exactly the way
 * CommandLineToArgvW will undo it. */
static void append_quoted(char *dst, size_t cap, const char *arg)
{
    size_t n = strlen(dst);
    int need = (*arg == '\0' || strpbrk(arg, " \t\"") != NULL);
    if (!need) {
        if (n + strlen(arg) + 1 < cap) strcat(dst, arg);
        return;
    }
    if (n + 2 < cap) { dst[n++] = '"'; dst[n] = '\0'; }
    size_t backslashes = 0;
    for (const char *p = arg; *p; p++) {
        if (*p == '\\') { backslashes++; continue; }
        if (*p == '"') {
            size_t k = backslashes * 2 + 1;
            while (k-- && n + 2 < cap) dst[n++] = '\\';
            if (n + 2 < cap) dst[n++] = '"';
            dst[n] = '\0';
            backslashes = 0;
            continue;
        }
        while (backslashes-- && n + 2 < cap) dst[n++] = '\\';
        dst[n] = '\0';
        backslashes = 0;
        if (n + 2 < cap) { dst[n++] = *p; dst[n] = '\0'; }
    }
    while (backslashes-- && n + 2 < cap) dst[n++] = '\\';
    if (n + 2 < cap) { dst[n++] = '"'; dst[n] = '\0'; }
}

hermes_pid_t hermes_spawn(char *const argv[])
{
    if (!argv || !argv[0]) { errno = EINVAL; return (hermes_pid_t)-1; }

    size_t cap = 4096;
    for (int i = 0; argv[i]; i++) cap += strlen(argv[i]) * 2 + 4;
    char *line = (char *)calloc(cap, 1);
    if (!line) { errno = ENOMEM; return (hermes_pid_t)-1; }
    for (int i = 0; argv[i]; i++) {
        if (i) strncat(line, " ", cap - strlen(line) - 1);
        append_quoted(line, cap, argv[i]);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);

    /* CreateProcess needs a writable command line and resolves argv[0] via PATH only if we
     * ask it to; search for the executable ourselves so a bare "git" works like execvp. */
    char exe[MAX_PATH];
    if (strchr(argv[0], '\\') || strchr(argv[0], '/')) {
        snprintf(exe, sizeof exe, "%s", argv[0]);
    } else if (!SearchPathA(NULL, argv[0], ".exe", (DWORD)sizeof exe, exe, NULL)) {
        snprintf(exe, sizeof exe, "%s", argv[0]);
    }

    BOOL ok = CreateProcessA(exe, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    free(line);
    if (!ok) { errno = ENOENT; return (hermes_pid_t)-1; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

hermes_pid_t hermes_spawn_detached(char *const argv[], const char *log_path)
{
    if (!argv || !argv[0]) { errno = EINVAL; return (hermes_pid_t)-1; }

    size_t cap = 4096;
    for (int i = 0; argv[i]; i++) cap += strlen(argv[i]) * 2 + 4;
    char *line = (char *)calloc(cap, 1);
    if (!line) { errno = ENOMEM; return (hermes_pid_t)-1; }
    for (int i = 0; argv[i]; i++) {
        if (i) strncat(line, " ", cap - strlen(line) - 1);
        append_quoted(line, cap, argv[i]);
    }

    /* Inheritable handles: CreateProcessA with bInheritHandles=TRUE only passes handles
     * created with bInheritHandle set. Without this the child ran with no stdout at all and
     * the log stayed empty -- caught by tests/test_posix_compat.c, not by the compiler. */
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;

    HANDLE out = INVALID_HANDLE_VALUE, in = INVALID_HANDLE_VALUE;
    if (log_path && *log_path) {
        out = CreateFileA(log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (out == INVALID_HANDLE_VALUE) {
        out = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    }
    in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = (in != INVALID_HANDLE_VALUE) ? in : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = out;
    si.hStdError = out;

    char exe[MAX_PATH];
    if (strchr(argv[0], '\\') || strchr(argv[0], '/')) snprintf(exe, sizeof exe, "%s", argv[0]);
    else if (!SearchPathA(NULL, argv[0], ".exe", (DWORD)sizeof exe, exe, NULL))
        snprintf(exe, sizeof exe, "%s", argv[0]);

    BOOL ok = CreateProcessA(exe, line, NULL, NULL, TRUE,
                             DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi);
    free(line);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
    if (!ok) { errno = ENOENT; return (hermes_pid_t)-1; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

hermes_pid_t hermes_spawn_in(const char *cwd, char *const argv[], int silent)
{
    if (!argv || !argv[0]) { errno = EINVAL; return (hermes_pid_t)-1; }

    size_t cap = 4096;
    for (int i = 0; argv[i]; i++) cap += strlen(argv[i]) * 2 + 4;
    char *line = (char *)calloc(cap, 1);
    if (!line) { errno = ENOMEM; return (hermes_pid_t)-1; }
    for (int i = 0; argv[i]; i++) {
        if (i) strncat(line, " ", cap - strlen(line) - 1);
        append_quoted(line, cap, argv[i]);
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    HANDLE nul = INVALID_HANDLE_VALUE;
    if (silent) {
        SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof sa);
        sa.nLength = sizeof sa;
        sa.bInheritHandle = TRUE;
        nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
        if (nul != INVALID_HANDLE_VALUE) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = nul;
            si.hStdError = nul;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }
    }

    char exe[MAX_PATH];
    if (strchr(argv[0], '\\') || strchr(argv[0], '/')) snprintf(exe, sizeof exe, "%s", argv[0]);
    else if (!SearchPathA(NULL, argv[0], ".exe", (DWORD)sizeof exe, exe, NULL))
        snprintf(exe, sizeof exe, "%s", argv[0]);

    BOOL ok = CreateProcessA(exe, line, NULL, NULL, silent ? TRUE : FALSE, 0, NULL,
                             (cwd && *cwd) ? cwd : NULL, &si, &pi);
    free(line);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { errno = ENOENT; return (hermes_pid_t)-1; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

/* CreateProcess hands back handles, but hermes_spawn returned only the pid. Open a handle
 * for waiting by pid; this keeps the public type identical to POSIX's pid_t. */
static HANDLE open_by_pid(hermes_pid_t pid)
{
    return OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
                       FALSE, pid);
}

int hermes_wait(hermes_pid_t pid, int timeout_ms, int *status)
{
    HANDLE h = open_by_pid(pid);
    if (!h) { errno = ESRCH; return HERMES_WAIT_ERROR; }
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD w = WaitForSingleObject(h, ms);
    if (w == WAIT_TIMEOUT) { CloseHandle(h); return HERMES_WAIT_TIMEOUT; }
    if (w != WAIT_OBJECT_0) { CloseHandle(h); errno = ECHILD; return HERMES_WAIT_ERROR; }
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    if (status) *status = (int)code;
    return HERMES_WAIT_EXITED;
}

int hermes_kill(hermes_pid_t pid)
{
    HANDLE h = open_by_pid(pid);
    if (!h) { errno = ESRCH; return -1; }
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    if (!ok) { errno = EPERM; return -1; }
    return 0;
}

/* ── dynamic loading ───────────────────────────────────────────────────────── */

static char g_dl_error[512];

void *hermes_dlopen(const char *path)
{
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        DWORD e = GetLastError();
        snprintf(g_dl_error, sizeof g_dl_error, "LoadLibrary(%s) failed: %lu",
                 path ? path : "(null)", (unsigned long)e);
        errno = ENOENT;
        return NULL;
    }
    g_dl_error[0] = '\0';
    return (void *)h;
}

void *hermes_dlsym(void *handle, const char *symbol)
{
    if (!handle || !symbol) { errno = EINVAL; return NULL; }
    FARPROC p = GetProcAddress((HMODULE)handle, symbol);
    if (!p) {
        snprintf(g_dl_error, sizeof g_dl_error, "GetProcAddress(%s) failed", symbol);
        errno = ENOENT;
        return NULL;
    }
    return (void *)(uintptr_t)p;
}

int hermes_dlclose(void *handle)
{
    if (!handle) { errno = EINVAL; return -1; }
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

const char *hermes_dlerror(void) { return g_dl_error; }

#else /* POSIX: the calls already exist, the names are the seam. */

#include <dlfcn.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

extern char **environ;

hermes_pid_t hermes_spawn(char *const argv[])
{
    pid_t pid = 0;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0) {
        errno = ENOENT;
        return (hermes_pid_t)-1;
    }
    return pid;
}

hermes_pid_t hermes_spawn_detached(char *const argv[], const char *log_path)
{
    pid_t pid = fork();
    if (pid < 0) return (hermes_pid_t)-1;
    if (pid == 0) {
        setsid();
        int null_in = open("/dev/null", O_RDONLY);
        if (null_in >= 0) { dup2(null_in, STDIN_FILENO); close(null_in); }
        if (log_path && *log_path) {
            int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
        } else {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    return pid;
}

hermes_pid_t hermes_spawn_in(const char *cwd, char *const argv[], int silent)
{
    pid_t pid = fork();
    if (pid < 0) return (hermes_pid_t)-1;
    if (pid == 0) {
        if (cwd && *cwd && chdir(cwd) != 0) _exit(127);
        if (silent) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    return pid;
}

int hermes_wait(hermes_pid_t pid, int timeout_ms, int *status)
{
    if (timeout_ms < 0) {
        int st = 0;
        if (waitpid(pid, &st, 0) < 0) return HERMES_WAIT_ERROR;
        if (status) *status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
        return HERMES_WAIT_EXITED;
    }
    for (int waited = 0; waited < timeout_ms; waited += 10) {
        int st = 0;
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) {
            if (status) *status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
            return HERMES_WAIT_EXITED;
        }
        if (r < 0) return HERMES_WAIT_ERROR;
        usleep(10 * 1000);
    }
    return HERMES_WAIT_TIMEOUT;
}

int hermes_kill(hermes_pid_t pid) { return kill(pid, SIGTERM); }

void *hermes_dlopen(const char *path) { return hermes_dlopen(path); }
void *hermes_dlsym(void *h, const char *s) { return hermes_dlsym(h, s); }
int   hermes_dlclose(void *h) { return hermes_dlclose(h); }
const char *hermes_dlerror(void) { return hermes_dlerror(); }

#endif /* _WIN32 */
