#ifndef NOC_W7_DELIVERYOUT_305AC3_H
#define NOC_W7_DELIVERYOUT_305AC3_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes cron/delivery_outcome.py (gate w7_deliveryout_305ac3).
 * Pure terminal-status decisions; the DB/queue writes stay in the caller. */

/* PoP: w7_deliveryout_terminal @ cron/delivery_outcome.py:_terminal
 * Given the queue row's status (and optional error) for the execution, returns
 * JSON null while queued/unknown-status, else [ledger_outcome, delivery_error_or_null].
 * _LEDGER_OUTCOME maps unknown->failed; failed/unknown derive
 * str(row.get("error") or f"delivery {row['status']}"). */
json_t *w7_deliveryout_terminal(const char *status, bool has_error, const char *error);

/* PoP: w7_deliveryout_settled_outcome @ cron/delivery_outcome.py:settled_outcome */
json_t *w7_deliveryout_settled_outcome(const char *status, bool has_error, const char *error);

/* PoP: w7_deliveryout_settle_queued @ cron/delivery_outcome.py:settle_queued_delivery
 * False when either id is missing or the row is not terminal; True otherwise. */
bool w7_deliveryout_settle_queued(bool has_job_id, const char *job_id,
                                  bool has_execution_id, const char *execution_id,
                                  const char *status, bool has_error, const char *error);

#endif /* NOC_W7_DELIVERYOUT_305AC3_H */
