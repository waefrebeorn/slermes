/*
 * noc_w17_kanbandbd_e96b7a.h -- public C API for the pure dispatch helpers of
 * upstream hermes_cli/kanban_db_dispatch.py (wave17 gate w17_kanbandbd_e96b7a).
 */
#ifndef NOC_W17_KANBANDBD_E96B7A_H
#define NOC_W17_KANBANDBD_E96B7A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (int(returncode) & 0xFF) << 8 -- Popen wait-status registry encoding. */
int w17_kanbandbd_e96b7a_wait_status_from_returncode(int returncode);

/* (kind, code): 0 clean_exit, 1 rate_limited, 2 terminal_provider, 3 nonzero_exit.
 * rate_limit_code / terminal_provider_code are the late-bound kanban constants.
 * Returns a borrowed static string; *out_code receives the code unchanged. */
const char *w17_kanbandbd_e96b7a_exit_code_kind(int code, int rate_limit_code,
                                                int terminal_provider_code, int *out_code);

/* error_text[:80] -> \bpid \d+\b -> "pid N" -> \b\d{10,}\b -> "<TS>" -> lower().strip().
 * Caller frees the returned string. */
char *w17_kanbandbd_e96b7a_error_fingerprint(const char *error_text);

/* term.get("host_local") and (attempted or refused) and not terminated.
 * `termination` is a JSON object of booleans. */
int w17_kanbandbd_e96b7a_worker_survived_termination(json_t *termination);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_KANBANDBD_E96B7A_H */
