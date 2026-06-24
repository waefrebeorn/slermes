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
