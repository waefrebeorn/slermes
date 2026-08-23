/*
 * update_receipt.h — C11 port of hermes_cli/update_receipt.py
 *
 * Structured update receipts (<HERMES_HOME>/logs/update_receipts/) +
 * post-update fleet version verification. Every public entry point is
 * exception-swallowing by construction; the module singleton gives the
 * updater exactly-once finalization.
 */
#ifndef SLERMES_UPDATE_RECEIPT_H
#define SLERMES_UPDATE_RECEIPT_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UpdateReceipt @ hermes_cli/update_receipt.py:UpdateReceipt */
typedef struct update_receipt update_receipt_t;

/* __init__ @ UpdateReceipt.__init__ */
update_receipt_t *update_receipt_new(void);
void update_receipt_free(update_receipt_t *receipt);

/* step / skip / gateway_restart_result / finalize methods. */
void update_receipt_step(update_receipt_t *r, const char *name, int ok,
                         const char *detail);
void update_receipt_skip(update_receipt_t *r, const char *name,
                         const char *reason);
void update_receipt_gateway_restart_result(
    update_receipt_t *r, json_t *restarted_services,
    json_t *relaunched_profiles, json_t *externally_supervised_profiles,
    const long *killed_pids, size_t killed_n, const char **failed_units,
    size_t failed_n, int incomplete, const char *phase_error);
void update_receipt_finalize(update_receipt_t *r, const char *outcome);

/* The receipt's data object (borrowed). */
json_t *update_receipt_data(update_receipt_t *r);

/* Module singleton accessors used by record_* wrappers. */
update_receipt_t *update_receipt_current(void);
void update_receipt_set_current(update_receipt_t *r);

/* _receipt_dir — malloc'd <home>/logs/update_receipts. Caller frees. */
char *update_receipt_dir(void);

/* begin_update_receipt / record_step / record_skip / record_gateway_restart
 * — never raise; no-op without an active receipt. */
void begin_update_receipt(void);
void record_step(const char *name, int ok, const char *detail);
void record_skip(const char *name, const char *reason);
void record_gateway_restart(json_t *restarted_services,
                            json_t *relaunched_profiles,
                            json_t *externally_supervised_profiles,
                            const long *killed_pids, size_t killed_n,
                            const char **failed_units, size_t failed_n,
                            int incomplete, const char *phase_error);

/* finalize_update_receipt — persist + pop the singleton. Returns a malloc'd
 * written path or NULL. Exactly-once via the popped singleton. */
char *finalize_update_receipt(const char *outcome, json_t *fleet,
                              const char *stop_reason);

/* finalize_pending_update_receipt — command-boundary safety net. exit_code:
 * 0 or -1 → success; 2 → refused; else failed. -1 means None. */
char *finalize_pending_update_receipt(int exit_code, const char *stop_reason);

/* _prune_old_receipts @ ...:_prune_old_receipts — keep last N=20. */
void prune_old_receipts(const char *directory);

/* read_latest_receipt — parsed latest.json dict, or NULL. Caller frees. */
json_t *read_latest_receipt(void);

/* collect_fleet_versions — one entry per live profile gateway:
 * {profile, pid, code_sha, code_version, state[, source]}. Caller frees. */
json_t *collect_fleet_versions(void);

/* print_fleet_version_matrix — prints matrix; returns 1 when any STALE. */
int print_fleet_version_matrix(json_t *fleet);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_UPDATE_RECEIPT_H */
