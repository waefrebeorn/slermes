/*
 * port_gateway.c — Port of Python hermes_cli/gateway.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
 * _capture_gateway_argv — Return the live argv of a running gateway process.
 *
 * Python: def _capture_gateway_argv(pid: int) -> list[str] | None:
 *   if pid <= 1: return None
 *   try: import psutil
 *   except ImportError: return None
 *   try: argv = list(psutil.Process(pid).cmdline() or [])
 *   except (NoSuchProcess, AccessDenied, ZombieProcess): return None
 *   if not argv: return None
 *   return argv
 *
 * In C: read /proc/<pid>/cmdline which is null-separated.
 */
#define MAX_ARGV_LEN 4096
#define MAX_ARGC 256

/* Port of Python: _capture_gateway_argv */
int _capture_gateway_argv(int pid, char** argv_out, int max_argc)
{
    if (pid <= 1 || !argv_out || max_argc <= 0) {
        return 0;
    }

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0; /* Process doesn't exist or no permission */
    }

    char buf[MAX_ARGV_LEN];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';

    /* Count and copy null-separated arguments */
    int argc = 0;
    size_t pos = 0;
    while (pos < (size_t)n && argc < max_argc) {
        argv_out[argc] = strdup(buf + pos);
        if (!argv_out[argc]) break;
        argc++;
        pos += strlen(buf + pos) + 1;
    }

    return argc;
}

/*
 * _capture_gateway_argv_to_json — Return gateway argv as JSON array.
 */
json_t* _capture_gateway_argv_json(int pid)
{
    char* argv[MAX_ARGC];
    int argc = _capture_gateway_argv(pid, argv, MAX_ARGC);

    json_t* result = json_new_array();
    if (!result) return NULL;

    for (int i = 0; i < argc; i++) {
        json_array_append(result, json_new_string(argv[i]));
        free(argv[i]);
    }

    return result;
}

/*
 * launch_detached_gateway_restart_by_cmdline — Launch a gateway restart.
 *
 * Python: launches a detached process with the given argv.
 */
/* Port of Python: launch_detached_gateway_restart_by_cmdline */
bool launch_detached_gateway_restart_by_cmdline(const char* run_argv[], int argc)
{
    if (!run_argv || argc <= 0) {
        hermes_log(LOG_WARNING, "port", "launch_detached_gateway_restart_by_cmdline: empty argv");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        hermes_log(LOG_ERROR, "port", "launch_detached_gateway_restart_by_cmdline: fork failed: %s", strerror(errno));
        return false;
    }

    if (pid == 0) {
        /* Child: become session leader, redirect stdio */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Build argv array */
        char** child_argv = calloc(argc + 1, sizeof(char*));
        for (int i = 0; i < argc; i++) {
            child_argv[i] = strdup(run_argv[i]);
        }
        child_argv[argc] = NULL;

        execvp(child_argv[0], child_argv);
        _exit(127);
    }

    hermes_log(LOG_INFO, "port", "Launched gateway restart pid=%d", pid);
    return true;
}

/*
 * _spawn_gateway_restart_watcher — Spawn a watcher that monitors gateway restart.
 */
/* Port of Python: _spawn_gateway_restart_watcher */
bool _spawn_gateway_restart_watcher(int old_pid, const char* run_argv[], int argc)
{
    if (old_pid <= 1) {
        return false;
    }

    /* Check the old process is still running */
    if (kill(old_pid, 0) < 0 && errno == ESRCH) {
        /* Old process is gone — launch the restart */
        return launch_detached_gateway_restart_by_cmdline(run_argv, argc);
    }

    /* Old process still running — don't interfere */
    hermes_log(LOG_WARNING, "port", "Gateway pid %d still running, not restarting", old_pid);
    return false;
}

/*
 * _guard_named_profile_under_multiplexer — Guard against conflicting profiles.
 *
 * Python: checks if a named profile is running under a multiplexer and prevents
 * conflicts.
 */
/* Port of Python: _guard_named_profile_under_multiplexer */
void _guard_named_profile_under_multiplexer(const char* profile_name, bool force)
{
    if (!profile_name || !profile_name[0]) {
        return;
    }

    /* Check for running gateway with this profile */
    char pid_path[256];
    snprintf(pid_path, sizeof(pid_path), "/tmp/hermes-gateway-%s.pid", profile_name);

    int fd = open(pid_path, O_RDONLY);
    if (fd < 0) {
        return 0; /* No PID file — no conflict */
    }

    char pid_str[32];
    ssize_t n = read(fd, pid_str, sizeof(pid_str) - 1);
    close(fd);

    if (n <= 0) return;
    pid_str[n] = '\0';

    int old_pid = atoi(pid_str);
    if (old_pid <= 1) return;

    /* Check if process is alive */
    if (kill(old_pid, 0) == 0) {
        if (force) {
            hermes_log(LOG_WARNING, "port", "Force-killing conflicting gateway pid=%d", old_pid);
            kill(old_pid, SIGTERM);
            usleep(500000);
            /* Check if it's still alive, use SIGKILL */
            if (kill(old_pid, 0) == 0) {
                kill(old_pid, SIGKILL);
            }
            unlink(pid_path);
            return;
        } else {
            hermes_log(LOG_WARNING, "port", "Gateway pid=%d already running for profile=%s", old_pid, profile_name);
            return; /* Conflict — Python raises RuntimeError, C logs warning */
        }
    }

    /* Stale PID file — remove it */
    unlink(pid_path);
    return;
}

/* ===========================================================================
 *  PID-resolution helpers — ported from hermes_cli/gateway.py
 *  These were REAL_GAP.
 * =========================================================================== */

/* PoP: get_parent_pid @ hermes_cli/gateway.py:_get_parent_pid */
/* Returns parent PID of pid, or -1 when unavailable (portable /proc walk). */
int get_parent_pid(int pid)
{
    if (pid <= 1) return -1;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    /* Format: pid (comm) state ppid ... — comm may contain spaces/parens,
     * so scan fields: skip pid, then read comm (balanced parens), then
     * state, then ppid is the 4th field. */
    int field = 0, ppid = -1;
    char buf[512];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    const char *p = buf;
    while (*p && field < 4) {
        /* skip to next field start */
        while (*p == ' ' || *p == '\t') p++;
        if (field == 1) {
            /* comm: ( ... ) possibly with spaces/parens inside */
            if (*p == '(') {
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '(') depth++;
                    else if (*p == ')') {
                        depth--;
                        if (depth == 0) { p++; break; }
                    }
                    p++;
                }
            } else {
                while (*p && *p != ' ' && *p != '\t') p++;
            }
            field = 2;
            continue;
        }
        /* read a single token */
        char tok[64] = {0};
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < (int)sizeof(tok)-1) tok[i++] = *p++;
        if (field == 0) field = 1;       /* pid consumed, next is comm */
        else if (field == 2) field = 3;  /* state consumed */
        else if (field == 3) { ppid = atoi(tok); field = 4; break; }
    }
    return (ppid > 0) ? ppid : -1;
}

/* PoP: _is_pid_ancestor_of_current_process @ hermes_cli/gateway.py:_is_pid_ancestor_of_current_process */
int is_pid_ancestor_of_current_process(int target_pid)
{
    if (target_pid <= 0) return 0;
    int pid = (int)getpid();
    for (int guard = 0; guard < 64; guard++) {
        if (pid == target_pid) return 1;
        int parent = get_parent_pid(pid);
        if (parent <= 0) break;
        pid = parent;
    }
    return 0;
}

/* PoP: get_ancestor_pids @ hermes_cli/gateway.py:_get_ancestor_pids */
/* Fills out[] with ancestor PIDs (including self); returns count (<=max). */
int get_ancestor_pids(int *out, int max)
{
    int pid = (int)getpid();
    int n = 0;
    for (int guard = 0; guard < 64 && n < max; guard++) {
        int found = 0;
        for (int k = 0; k < n; k++) if (out[k] == pid) { found = 1; break; }
        if (found) break;
        out[n++] = pid;
        int parent = get_parent_pid(pid);
        if (parent <= 0) break;
        pid = parent;
    }
    return n;
}

/* PoP: append_unique_pid @ hermes_cli/gateway.py:_append_unique_pid */
/* Appends pid to *pids (growing the array) if valid/unique/not excluded. */
void append_unique_pid(int **pids, int *count, int *cap, int pid,
                       const int *exclude, int exclude_n)
{
    if (pid <= 0) return;
    if (pid == (int)getpid()) return;
    for (int i = 0; i < exclude_n; i++) if (exclude[i] == pid) return;
    for (int i = 0; i < *count; i++) if ((*pids)[i] == pid) return;
    if (*count >= *cap) {
        int ncap = (*cap == 0) ? 8 : *cap * 2;
        int *np = realloc(*pids, ncap * sizeof(int));
        if (!np) return;
        *pids = np; *cap = ncap;
    }
    (*pids)[(*count)++] = pid;
}

/* ===========================================================================
 *  systemd / restart helpers — ported from hermes_cli/gateway.py
 *  These were REAL_GAP.
 * =========================================================================== */

/* PoP: supports_systemd_services @ hermes_cli/gateway.py:supports_systemd_services */
/* Return true if systemd is available and usable on this platform.
 * Mirrors the Python: not termux, systemctl in PATH, WSL/container checks.
 * On non-Linux or Termux, returns false. */
bool supports_systemd_services(void)
{
    /* Not Linux → false */
    FILE *f = popen("uname -s 2>/dev/null", "r");
    if (!f) return false;
    char buf[64];
    bool is_linux = false;
    if (fgets(buf, sizeof(buf), f)) {
        buf[strcspn(buf, "\n")] = 0;
        if (0 == strcmp(buf, "Linux"))
            is_linux = true;
    }
    pclose(f);
    if (!is_linux)
        return false;

    /* Termux check: /data/data/com.termux prefix */
    if (access("/data/data/com.termux", F_OK) == 0)
        return false;

    /* systemctl must exist in PATH */
    if (system("which systemctl >/dev/null 2>&1") != 0)
        return false;

    /* WSL check: /proc/version contains "microsoft" */
    FILE *vf = fopen("/proc/version", "r");
    if (vf) {
        char vbuf[512];
        bool is_wsl = false;
        if (fgets(vbuf, sizeof(vbuf), vf)) {
            if (strstr(vbuf, "microsoft") || strstr(vbuf, "Microsoft"))
                is_wsl = true;
        }
        fclose(vf);
        if (is_wsl) {
            /* _wsl_systemd_operational: check systemd is actually running */
            return (access("/run/systemd/system", F_OK) == 0);
        }
    }

    /* Container check: /.dockerenv or /run/.containerenv */
    if (access("/.dockerenv", F_OK) == 0 || access("/run/.containerenv", F_OK) == 0) {
        /* _container_systemd_operational: check user or system scope works */
        if (access("/run/systemd/system", F_OK) == 0)
            return true;
        char *xdg = getenv("XDG_RUNTIME_DIR");
        if (xdg) {
            char path[512];
            snprintf(path, sizeof(path), "%s/systemd", xdg);
            if (access(path, F_OK) == 0)
                return true;
        }
        return false;
    }

    return true;
}

/* PoP: get_service_pids @ hermes_cli/gateway.py:_get_service_pids */
/* Return PIDs currently managed by systemd or launchd gateway services.
 * Fills pids[] array (caller-allocated, max elements), returns count.
 * Uses popen to run systemctl list-units + show --property=MainPID. */
int get_service_pids(int *pids, int max)
{
    int count = 0;
    if (!pids || max <= 0)
        return 0;

    if (supports_systemd_services()) {
        /* Try both scopes: --user and system */
        const char *scopes[2] = {"systemctl --user", "systemctl"};
        for (int s = 0; s < 2; s++) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd),
                     "%s list-units 'hermes-gateway*' --plain --no-legend --no-pager 2>/dev/null",
                     scopes[s]);
            FILE *fp = popen(cmd, "r");
            if (!fp)
                continue;
            char line[256];
            while (fgets(line, sizeof(line), fp) && count < max) {
                /* Parse first field: service name ending in .service */
                char *space = strchr(line, ' ');
                if (space) *space = 0;
                char *nl = strchr(line, '\n');
                if (nl) *nl = 0;
                if (strlen(line) < 8 ||
                    strcmp(line + strlen(line) - 8, ".service") != 0)
                    continue;
                char svc[128];
                snprintf(svc, sizeof(svc), "%s", line);

                /* Get MainPID */
                char show_cmd[512];
                snprintf(show_cmd, sizeof(show_cmd),
                         "%s show '%s' --property=MainPID --value 2>/dev/null",
                         scopes[s], svc);
                FILE *sfp = popen(show_cmd, "r");
                if (!sfp)
                    continue;
                char pid_buf[32];
                if (fgets(pid_buf, sizeof(pid_buf), sfp)) {
                    pid_buf[strcspn(pid_buf, "\n")] = 0;
                    int pid = atoi(pid_buf);
                    if (pid > 0) {
                        /* Deduplicate */
                        bool found = false;
                        for (int i = 0; i < count; i++)
                            if (pids[i] == pid) { found = true; break; }
                        if (!found && count < max)
                            pids[count++] = pid;
                    }
                }
                pclose(sfp);
            }
            pclose(fp);
        }
    }

    /* launchd (macOS) path: check if we're on macOS via uname */
    FILE *mf = popen("uname -s 2>/dev/null", "r");
    if (mf) {
        char mb[16];
        if (fgets(mb, sizeof(mb), mf)) {
            mb[strcspn(mb, "\n")] = 0;
            if (0 == strcmp(mb, "Darwin") && count < max) {
                /* launchctl list <label> — get_launchd_label returns the label */
                /* We don't have get_launchd_label in C; skip launchd path
                 * (the systemd path is the primary on Linux/WSL/containers) */
            }
        }
        pclose(mf);
    }

    return count;
}

/* PoP: request_gateway_self_restart @ hermes_cli/gateway.py:_request_gateway_self_restart */
/* Ask a running gateway ancestor to restart itself asynchronously.
 * Returns true if SIGUSR1 was sent, false if not possible.
 * Mirrors Python: check SIGUSR1 exists, check pid is ancestor, os.kill. */
bool request_gateway_self_restart(int pid)
{
#ifdef SIGUSR1
    if (pid <= 0)
        return false;
    if (!is_pid_ancestor_of_current_process(pid))
        return false;
    if (kill(pid, SIGUSR1) == 0)
        return true;
    /* ESRCH = process not found (already gone) — Python returns True */
    if (errno == ESRCH)
        return true;
    return false;
#else
    return false;
#endif
}

/* PoP: graceful_restart_via_sigusr1 @ hermes_cli/gateway.py:_graceful_restart_via_sigusr1 */
/* Send SIGUSR1 to a gateway PID and wait for it to exit gracefully.
 * Polls /proc/<pid> existence every 0.5s up to drain_timeout seconds.
 * Returns true if the process exited within the timeout. */
bool graceful_restart_via_sigusr1(int pid, double drain_timeout)
{
#ifdef SIGUSR1
    if (pid <= 0)
        return false;

    int rc = kill(pid, SIGUSR1);
    if (rc != 0) {
        if (errno == ESRCH)
            return true; /* Already gone — nothing to drain */
        return false;    /* Permission error or other */
    }

    /* Wait for the process to exit.
     * drain_timeout is in seconds; minimum 1.0 per Python max(drain_timeout, 1.0) */
    double timeout = drain_timeout > 1.0 ? drain_timeout : 1.0;
    double elapsed = 0.0;
    const double poll_interval = 0.5;

    while (elapsed < timeout) {
        /* Check if process still exists via /proc/<pid> */
        char proc_path[64];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
        if (access(proc_path, F_OK) != 0)
            return true; /* Process exited */

        /* Also check via kill(pid, 0) — no signal sent */
        if (kill(pid, 0) != 0 && errno == ESRCH)
            return true;

        /* Sleep poll_interval seconds (500ms) */
        usleep((useconds_t)(poll_interval * 1000000));
        elapsed += poll_interval;
    }
    /* Drain didn't finish in time */
    return false;
#else
    (void)pid;
    (void)drain_timeout;
    return false;
#endif
}
