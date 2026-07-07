/*
 * port_gateway_windows.c — Port of Python
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>

/* Resolve the gateway pidfile path (mirrors gateway_lifecycle.c). */
static void _gw_pidfile_path(char *out, size_t n)
{
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { out[0] = '\0'; return; }
    snprintf(out, n, "%s/.slermes/gateway.pid", home);
}

/* Real check: is the gateway process still alive? Read the pidfile and probe
 * the PID with kill(pid, 0). Returns true if the process is absent. */
static bool _gateway_process_absent(void)
{
    char path[1024];
    _gw_pidfile_path(path, sizeof(path));
    if (path[0] == '\0') return true;

    FILE *f = fopen(path, "r");
    if (!f) return true;  /* no pidfile => not running */
    long pid = -1;
    if (fscanf(f, "%ld", &pid) != 1) { fclose(f); return true; }
    fclose(f);

    if (pid <= 0) return true;
    /* kill(pid, 0) returns 0 if the process exists, ESRCH if absent. */
    return (kill((pid_t)pid, 0) != 0);
}

/* Port of Python: _wait_for_gateway_absent */
bool _wait_for_gateway_absent(void* ctx, void* timeout_s, void* interval_s)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_wait_for_gateway_absent: null context");
        return false;
    }

    /* Wait for gateway process to be absent */
    int elapsed = 0;
    int timeout = (timeout_s && *(int*)timeout_s) ? *(int*)timeout_s : 30;
    int interval = (interval_s && *(int*)interval_s) ? *(int*)interval_s : 1;

    while (elapsed < timeout) {
        if (_gateway_process_absent()) {
            hermes_log(LOG_INFO, "port", "_wait_for_gateway_absent: gateway absent confirmed");
            return true;
        }
        hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent: waiting... elapsed=%d", elapsed);
        sleep(interval);
        elapsed += interval;
    }
    hermes_log(LOG_WARNING, "port", "_wait_for_gateway_absent: timeout reached");
    return false;
}