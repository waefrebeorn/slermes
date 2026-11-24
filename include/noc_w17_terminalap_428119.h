#ifndef NOC_W17_TERMINALAP_428119_H
#define NOC_W17_TERMINALAP_428119_H

#include "hermes_json.h"

/* Replays the pure decision logic of agent/terminal_approval_batch.py.
 * All concurrency/context is supplied by the caller as data. */

/* terminal_approval_runs: ``calls`` is an array of
 * {"tag":str,"name":str,"parse_error":<null|any>}; a call is a terminal iff
 * name=="terminal" and parse_error is null. Returns an array of arrays of tags
 * grouping consecutive calls with the same verdict; caller owns it. */
json_t *w17_terminalap_428119_terminal_approval_runs(json_t *calls);

/* consume_prepared_guard: slot/context state supplied as data. Returns a copy
 * of the consumed decision object, or JSON null when nothing is consumed. */
json_t *w17_terminalap_428119_consume_prepared_guard(
    int slot_exists, int preparing, int failure_seen,
    const char *ctx_call_id, const char *expected_call_id,
    const char *command, const char *env_type, int has_host_access,
    const char *guard_command, const char *guard_env_type, int guard_host_access,
    json_t *decision, const char *approval_mode);

/* preparing_terminal_approval() */
int w17_terminalap_428119_preparing_terminal_approval(int slot_exists, int preparing);

#endif
