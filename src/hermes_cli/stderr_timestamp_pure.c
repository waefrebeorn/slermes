#define _POSIX_C_SOURCE 200809L
/* stderr_timestamp_pure.c — Faithful C11 port of the pure, deterministic
 * helpers in hermes_cli/stderr_timestamp.py (run a child while prefixing
 * each stderr line with a logging-style timestamp).
 *
 * Ports (PoP):
 *   _TIMESTAMP_PREFIX.match          -> stderr_ts_has_timestamp_prefix

 *   _timestamp                        -> stderr_ts_timestamp (non-oracled; uses now())
 *   _command_exit_code                -> stderr_ts_command_exit_code
 *   _is_launchd_supervised            -> stderr_ts_is_launchd_supervised
 *   _is_hermes_gateway_run_argv       -> stderr_ts_is_gateway_run_argv
 *   _with_external_supervisor_flag    -> stderr_ts_with_external_supervisor_flag
 *   _prepare_child_command            -> stderr_ts_prepare_child_command
 *
 * Time/IO-bound helpers (_write_timestamped_line, _copy_stderr_with_timestamps,
 * _install_signal_forwarders, _restore_signal_handlers, main) are not ported
 * here — they are process/signal/IO glue, not pure logic.
 *
 * Reuses gateway_status.looks_like_gateway_command_line (already ported in
 * gateway/status.c) for the gateway-argv check, exactly as the Python does.
 */

#include "hermes_core_types.h"
#include "gateway_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STS_EXTERNAL_SUPERVISOR_FLAG "--external-supervisor"

/* ── _timestamp() ──────────────────────────────────────────────────
 * Mirrors logging.Formatter's %(asctime)s shape with millisecond precision:
 *   "%Y-%m-%d %H:%M:%S,%f"[:23]  -> "YYYY-MM-DD HH:MM:SS,mmm"
 * (Python truncates the 6-digit %f to 3 digits via [:23].)
 */
void stderr_ts_timestamp(char *buf, size_t cap) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    int ms = (int)(ts.tv_nsec / 1000000);
    snprintf(buf, cap, "%04d-%02d-%02d %02d:%02d:%02d,%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
}

/* ── _TIMESTAMP_PREFIX.match ───────────────────────────────────────
 * ^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}(?:\s|$)
 * Returns 1 if the rendered line already carries a logging timestamp prefix
 * (so we must NOT double-prefix it).
 *//* PoP: stderr_ts_has_timestamp_prefix @ hermes_cli/stderr_timestamp.py:_TIMESTAMP_PREFIX.match */

int stderr_ts_has_timestamp_prefix(const char *line) {
    if (!line) return 0;
    /* 4 digits */
    for (int i = 0; i < 4; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[4] != '-') return 0;
    for (int i = 5; i < 7; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[7] != '-') return 0;
    for (int i = 8; i < 10; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[10] != ' ') return 0;
    for (int i = 11; i < 13; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[13] != ':') return 0;
    for (int i = 14; i < 16; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[16] != ':') return 0;
    for (int i = 17; i < 19; i++) if (line[i] < '0' || line[i] > '9') return 0;
    if (line[19] != ',') return 0;
    for (int i = 20; i < 23; i++) if (line[i] < '0' || line[i] > '9') return 0;
    char c = line[23];
    if (c != '\0' && c != ' ' && c != '\t' && c != '\r' && c != '\n')
        return 0;
    return 1;
}

/* ── _command_exit_code ──────────────────────────────────────────── *//* PoP: stderr_ts_command_exit_code @ hermes_cli/stderr_timestamp.py:_command_exit_code */

int stderr_ts_command_exit_code(int returncode) {
    if (returncode < 0) return 128 + (-returncode);
    return returncode;
}

/* ── _is_launchd_supervised ────────────────────────────────────────
 * True when XPC_SERVICE_NAME is set and not "0".
 *//* PoP: stderr_ts_is_launchd_supervised @ hermes_cli/stderr_timestamp.py:_is_launchd_supervised */

int stderr_ts_is_launchd_supervised(const char *xpc_service_name) {
    if (!xpc_service_name || !*xpc_service_name) return 0;
    /* Python .strip() then bool(... and != "0") */
    /* strip leading/trailing whitespace */
    while (*xpc_service_name == ' ' || *xpc_service_name == '\t') xpc_service_name++;
    size_t l = strlen(xpc_service_name);
    while (l > 0 && (xpc_service_name[l - 1] == ' ' || xpc_service_name[l - 1] == '\t')) l--;
    if (l == 0) return 0;
    /* compare first l chars to "0" */
    if (l == 1 && xpc_service_name[0] == '0') return 0;
    return 1;
}

/* ── _is_hermes_gateway_run_argv ────────────────────────────────── *//* PoP: stderr_ts_is_gateway_run_argv @ hermes_cli/stderr_timestamp.py:_is_hermes_gateway_run_argv */

int stderr_ts_is_gateway_run_argv(char **argv, int argc) {
    /* Python: " ".join(str(part) for part in command) then
     * looks_like_gateway_command_line(...) */
    size_t total = 1;
    for (int i = 0; i < argc; i++) total += strlen(argv[i]) + 1;
    char *joined = (char *)malloc(total);
    joined[0] = '\0';
    for (int i = 0; i < argc; i++) {
        if (i) strcat(joined, " ");
        strcat(joined, argv[i]);
    }
    int r = gwstatus_looks_like_gateway_command_line(joined);
    free(joined);
    return r;
}

/* ── _with_external_supervisor_flag ─────────────────────────────── *//* PoP: stderr_ts_with_external_supervisor_flag @ hermes_cli/stderr_timestamp.py:_with_external_supervisor_flag */

void stderr_ts_with_external_supervisor_flag(char **argv, int argc,
                                             char **out, int *outc) {
    int has = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], STS_EXTERNAL_SUPERVISOR_FLAG) == 0) { has = 1; break; }
    }
    int j = 0;
    for (int i = 0; i < argc; i++) out[j++] = argv[i];
    if (!has) out[j++] = (char *)STS_EXTERNAL_SUPERVISOR_FLAG;
    *outc = j;
}

/* ── _prepare_child_command ─────────────────────────────────────── *//* PoP: stderr_ts_prepare_child_command @ hermes_cli/stderr_timestamp.py:_prepare_child_command */

void stderr_ts_prepare_child_command(char **argv, int argc,
                                     const char *xpc_service_name,
                                     char **out, int *outc) {
    if (!stderr_ts_is_launchd_supervised(xpc_service_name) ||
        !stderr_ts_is_gateway_run_argv(argv, argc)) {
        int j = 0;
        for (int i = 0; i < argc; i++) out[j++] = argv[i];
        *outc = j;
        return;
    }
    stderr_ts_with_external_supervisor_flag(argv, argc, out, outc);
}
