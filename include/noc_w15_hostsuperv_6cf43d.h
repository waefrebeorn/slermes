#ifndef NOC_W15_HOSTSUPERV_6CF43D_H
#define NOC_W15_HOSTSUPERV_6CF43D_H
/*
 * Public C API for the port of tui_gateway/host_supervisor.py
 * (gate w15_hostsuperv_6cf43d). All exported symbols are gate-prefixed.
 *
 * PoP: w15_hostsuperv_6cf43d_is_compute_host_identity @ tui_gateway/host_supervisor.py:is_compute_host_identity
 * PoP: w15_hostsuperv_6cf43d_log_record_text         @ tui_gateway/host_supervisor.py:append_log_record
 * PoP: w15_hostsuperv_6cf43d_validate_hello          @ tui_gateway/host_supervisor.py:HostSupervisor._validate_hello
 *
 * The module's process-management side effects are not ported. The three
 * deterministic decisions are: the compute-host command-line identity test,
 * the exactly-one-record log text, and the hello handshake validation.
 * The command string is supplied by the caller (no /proc or ps probing).
 */
#include "hermes_json.h"

int  w15_hostsuperv_6cf43d_is_compute_host_identity(const char *command);
char *w15_hostsuperv_6cf43d_log_record_text(const char *record);

/* Returns json_null() when the hello is accepted, else a json_string of the
 * RuntimeError message. Caller frees. */
json_t *w15_hostsuperv_6cf43d_validate_hello(int hello_present,
        const char *hello_home, const char *hello_sha,
        const char *expected_home, const char *expected_sha);

#endif /* NOC_W15_HOSTSUPERV_6CF43D_H */
