/*
 * noc_w34_reconcile_7686f7_p2.h - C surface for upstream
 * persistence/sqlite/admin/_reconcile.py (gate w34_reconcile_7686f7_p2).
 *
 * The value predicates and budget arithmetic of the browse reconcile: is the
 * store still canonical enough to touch, what do PRAGMA integrity_check rows
 * add up to, how much of the one deadline is left, and the tuning constants
 * that deadline is made of.  sqlite3 itself, the monotonic clock and the
 * progress-handler registration stay in the caller.
 */
#ifndef NOC_W34_RECONCILE_7686F7_P2_H
#define NOC_W34_RECONCILE_7686F7_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* names = the table names sqlite_master reported ->
 * {"ok": true, "error": null} when both canonical history tables exist, else
 * {"ok": false, "error": <the "use sessions recover" refusal>}. */
json_t *w34_reconcile_7686f7_p2_history_tables(const json_t *names);

/* rows = PRAGMA integrity_check(3) result rows -> the first three problems
 * joined with "; ", or null when every row said "ok" (nothing to report). */
json_t *w34_reconcile_7686f7_p2_integrity_problem(const json_t *rows);

/* {"remaining": max(deadline - now, 0.0), "busy_timeout": max(remaining, 0.05)} */
json_t *w34_reconcile_7686f7_p2_budget(double deadline, double now);

/* {"raise": <expired or remaining <= 0>, "message": <refusal or null>} */
json_t *w34_reconcile_7686f7_p2_budget_check(int expired, double remaining);

/* The browse budget constants, as an object. */
json_t *w34_reconcile_7686f7_p2_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W34_RECONCILE_7686F7_P2_H */
