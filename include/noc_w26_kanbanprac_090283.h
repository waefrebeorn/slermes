/*
 * noc_w26_kanbanprac_090283.h - C surface for upstream
 * hermes_cli/kanban_pr_acceptance.py (gate w26_kanbanprac_090283).
 *
 * Exact-head GitHub acceptance for explicitly declared PR tasks.  ``gh api``
 * calls, the routed subprocess env, and the SQLite receipt write are seams; the
 * port owns the two value decisions that surround them: which completion
 * contracts are legal, and how one check run / legacy status is classified
 * against the exact head the receipt captured.
 */
#ifndef NOC_W26_KANBANPRAC_090283_H
#define NOC_W26_KANBANPRAC_090283_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* validate_contract(value) -> {"ok": true, "value": ...} or
 * {"ok": false, "error": "..."} (upstream raises ValueError). */
json_t *w26_kanbanprac_090283_validate_contract(const json_t *value);

/* _classify(check, sha, outcome, is_run) over a check-run or legacy status
 * object: "success" | "failure" | "infra" | "pending" | "stale". */
json_t *w26_kanbanprac_090283_classify(const json_t *check, const char *sha, int is_run);

/* The gh login/transport key tables and the contract regexes, as an object. */
json_t *w26_kanbanprac_090283_constants(void);

#ifdef __cplusplus
}
#endif
#endif
