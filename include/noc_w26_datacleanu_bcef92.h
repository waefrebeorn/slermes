/*
 * noc_w26_datacleanu_bcef92.h - C surface for upstream
 * hermes_cli/data_cleanup_holders.py (gate w26_datacleanu_bcef92).
 *
 * The decision core of the "is this home quiescent enough to wipe?" gate: who
 * answered the gateway's pause request, which supervisor owns a gateway (and
 * what the operator should stop), the drain deadline the pause response asks
 * for, and the per-row verdicts the backend-ownership ledger and the cron
 * execution ledger produce.  Every probe (control socket, kill(0), start time,
 * sqlite read) is a seam: the caller passes its answer in as a value.
 */
#ifndef NOC_W26_DATACLEANU_BCEF92_H
#define NOC_W26_DATACLEANU_BCEF92_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* pause_acked(response, identity) -> bool: the identified gateway itself
 * accepted (or is already running) the drain. */
json_t *w26_datacleanu_bcef92_pause_acked(const json_t *response, const json_t *identity);

/* owner_stop_hint(owner) -> str: the operator-facing "stop it through ..."
 * advice for the supervisor that owns a gateway. */
json_t *w26_datacleanu_bcef92_owner_stop_hint(const json_t *owner);

/* drain_timeout(response) -> number: min(120, max(5, drain_timeout + 5)). */
json_t *w26_datacleanu_bcef92_drain_timeout(const json_t *response);

/* backend_owner_state(row, running) -> "skip" | "invalid_pid" | "running" | "free"
 * for one backend-ownership ledger row; `running` is entry_still_running(). */
json_t *w26_datacleanu_bcef92_backend_owner_state(const json_t *row, int running);

/* cron_owner_state(row, pid_exists, current_start) -> "handoff" | "skip" |
 * "owns" | "free" for one claimed/running cron execution row;
 * `current_start` is get_process_start_time(pid), null when the probe failed. */
json_t *w26_datacleanu_bcef92_cron_owner_state(const json_t *row, int pid_exists,
                                               const json_t *current_start);

/* The supervisor -> advice table, as an object. */
json_t *w26_datacleanu_bcef92_constants(void);

#ifdef __cplusplus
}
#endif
#endif
