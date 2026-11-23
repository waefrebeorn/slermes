#ifndef NOC_W15_STDINRECO_6979CA_H
#define NOC_W15_STDINRECO_6979CA_H
/*
 * Public C API for the port of tui_gateway/_stdin_recovery.py
 * (gate w15_stdinreco_6979ca). All exported symbols are gate-prefixed.
 *
 * PoP: w15_stdinreco_6979ca_diagnose_stdin_state @ tui_gateway/_stdin_recovery.py:diagnose_stdin_state
 * PoP: w15_stdinreco_6979ca_handle_spurious_eof  @ tui_gateway/_stdin_recovery.py:handle_spurious_eof
 *
 * The POSIX fcntl/socket probes are environment reads that a pure C port
 * cannot perform on Windows. They are resolved-fact seams: the caller injects
 * the flag state, the diagnostic string and the recovery timestamps; the
 * window filter, rate cap, log strings and return value are reproduced exactly.
 */
#include "hermes_json.h"

/* Returns a newly allocated diagnostic string (caller frees).
 * fgetfl_error != NULL => "F_GETFL error: <err>"; otherwise O_NONBLOCK bit
 * of `flags` under `nonblock_flag`. rcvtimeout_repr != NULL is appended as
 * ", SO_RCVTIMEO=<repr>". NULL inputs model Python None. */
char *w15_stdinreco_6979ca_diagnose_stdin_state(int fcntl_available,
        long long flags, long long nonblock_flag,
        const char *fgetfl_error, const char *rcvtimeout_repr);

/* Returns {"recovered": bool, "logs": [str], "recovery_times": [num]}
 * (caller frees). `nonblock` models _stdin_nonblock(); `now` is time.time();
 * `max_recoveries` is MAX_RECOVERIES_PER_MINUTE; `diag_state` is the
 * diagnose_stdin_state() string used in the recovery log. */
json_t *w15_stdinreco_6979ca_handle_spurious_eof(int nonblock,
        const json_t *recovery_times, double now, int max_recoveries,
        const char *diag_state);

#endif /* NOC_W15_STDINRECO_6979CA_H */
