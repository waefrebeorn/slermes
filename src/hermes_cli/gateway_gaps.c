/* ───────────────────────── gap fillers ────────────────────────── */
/* Implements hermes_cli/gateway.py gap functions that have no C
 * equivalent yet (DA-1, c_location=None).                               */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "hermes_logger.h"

/* ================================================================== */
/*  Path / runtime helpers                                            */
/* ================================================================== */

/* PoP: gw_path_exists_safe @ hermes_cli/gateway.py:_path_exists_safe */
/* Path.exists() that treats an inaccessible path as absent.
 * EACCES/EIO/etc → false instead of propagating. */
bool gw_path_exists_safe(const char *path)
{
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) == 0) return true;
    /* ENOENT/ENOTDIR/EBADF/ELOOP → absent. EACCES → also treat as absent. */
    return false;
}

/* PoP: gw_runtime_dir_is_ours @ hermes_cli/gateway.py:_runtime_dir_is_ours */
/* True when runtime_dir exists and is owned by the current uid.
 * A leaked XDG_RUNTIME_DIR from another user must not be trusted. */
bool gw_runtime_dir_is_ours(const char *runtime_dir)
{
    if (!runtime_dir || !*runtime_dir) return false;
    struct stat st;
    if (stat(runtime_dir, &st) != 0) return false;
    return st.st_uid == getuid();
}

/* ================================================================== */
/*  systemd helpers                                                   */
/* ================================================================== */

/* PoP: gw_systemd_restart_wait_timeout @ hermes_cli/gateway.py:_systemd_restart_wait_timeout */
/* Cover systemd's relaunch delays before applying the runtime wait floor.
 * Reads RestartUSec + TimeoutStartUSec via systemctl show. */
double gw_systemd_restart_wait_timeout(bool system_mode)
{
    const char *unit = getenv("HERMES_GATEWAY_UNIT");
    if (!unit) unit = "hermes-gateway.service";
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "systemctl%s show '%s' -p RestartUSec -p TimeoutStartUSec --value 2>/dev/null",
             system_mode ? " --system" : "", unit);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0.0;
    char val1[64] = {0}, val2[64] = {0};
    /* Read two values (microseconds each) */
    if (fgets(val1, sizeof(val1), fp)) {
        if (fgets(val2, sizeof(val2), fp)) {
            double us1 = atof(val1);
            double us2 = atof(val2);
            pclose(fp);
            /* Return the larger of the two, in seconds */
            double max_us = us1 > us2 ? us1 : us2;
            return max_us > 0 ? max_us / 1000000.0 : 0.0;
        }
        /* Only one value read */
        double us = atof(val1);
        pclose(fp);
        return us > 0 ? us / 1000000.0 : 0.0;
    }
    pclose(fp);
    return 0.0;
}

/* ================================================================== */
/*  Process management                                                */
/* ================================================================== */

/* PoP: gw_wait_for_pid_exit @ hermes_cli/gateway.py:_wait_for_pid_exit */
/* Wait for a PID to exit (polling /proc/PID). Returns true if the
 * process exited within the timeout, false if still running. */
bool gw_wait_for_pid_exit(pid_t pid, double timeout_s)
{
    if (pid <= 0) return true;
    char path[64];
    struct stat st;
    double elapsed = 0.0;
    double interval = 0.1;
    while (elapsed < timeout_s) {
        snprintf(path, sizeof(path), "/proc/%d", pid);
        if (stat(path, &st) != 0) return true;
        struct timespec ts = {0, (long)(interval * 1e9)};
        nanosleep(&ts, NULL);
        elapsed += interval;
        /* Back off up to 1s */
        if (interval < 1.0) interval *= 1.5;
    }
    /* Final check */
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return stat(path, &st) != 0;
}

/* PoP: gw_escalate_wedged_gateway @ hermes_cli/gateway.py:_escalate_wedged_gateway */
/* Escalate a wedged gateway: SIGTERM → wait → SIGKILL. */
void gw_escalate_wedged_gateway(pid_t pid, double grace_s)
{
    if (pid <= 0) return;
    /* Try graceful SIGTERM first */
    kill(pid, SIGTERM);
    /* Wait for exit */
    if (!gw_wait_for_pid_exit(pid, grace_s)) {
        /* Force kill */
        kill(pid, SIGKILL);
        gw_wait_for_pid_exit(pid, 5.0);
    }
    hermes_log(LOG_WARNING, "gateway",
               "escalated wedged gateway PID %d (SIGTERM→SIGKILL after %.1fs)",
               pid, grace_s);
}

/* ================================================================== */
/*  Launchd / macOS helpers                                           */
/* ================================================================== */

/* PoP: gw_launchd_service_registered @ hermes_cli/gateway.py:_launchd_service_registered */
/* Check if a launchd service is registered for the given label. */
bool gw_launchd_service_registered(const char *label)
{
    if (!label || !*label) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "launchctl list '%s' 2>/dev/null", label);
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char line[256];
    bool found = false;
    if (fgets(line, sizeof(line), fp)) {
        /* launchctl list <label> outputs "PID\tStatus\tLabel" or just "Label" */
        if (strstr(line, label)) found = true;
    }
    pclose(fp);
    return found;
}

/* ================================================================== */
/*  Timestamped stderr                                                */
/* ================================================================== */

/* PoP: gw_timestamped_stderr_gateway_command @ hermes_cli/gateway.py:_timestamped_stderr_gateway_command */
/* Build a shell command that prefixes stderr lines with a timestamp.
 * Returns malloc'd string (caller frees). */
char *gw_timestamped_stderr_gateway_command(const char *base_cmd)
{
    if (!base_cmd || !*base_cmd) return NULL;
    /* Wrap: 2> >(while IFS= read -r l; do echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $l"; done) */
    const char *fmt = "2> >(while IFS= read -r line; do echo \"[$(date -u +%%Y-%%m-%%dT%%H:%%M:%%SZ)] $line\"; done)";
    /* Need to escape % for snprintf */
    const char *real_fmt = "2> >(while IFS= read -r line; do echo \"[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $line\"; done)";
    size_t cap = strlen(base_cmd) + strlen(real_fmt) + 32;
    char *cmd = malloc(cap);
    if (cmd) {
        snprintf(cmd, cap, "%s %s", base_cmd, real_fmt);
    }
    return cmd;
}
