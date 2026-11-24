/*
 * noc_w24_kanbanprac_15da5e.h - C surface for upstream
 * hermes_cli/kanban_pr_acceptance_store.py (gate w24_kanbanprac_15da5e).
 *
 * The acceptance decision a kanban card's completion makes before and after the
 * terminal write: whether this task/run/contract may be accepted at all, the
 * exact-GitHub-PR-URL matcher the contract is compared through, the contract
 * validator, the receipt recorder's return value + failure text, and the
 * literals the rules are written in.
 *
 * The SQLite connection, the write transaction, the event append and the `gh`
 * acceptance run are seams.  They arrive as values here: the snapshot row, the
 * assignee, and the receipt dict.
 */
#ifndef W24_KANBANPRAC_15DA5E_H
#define W24_KANBANPRAC_15DA5E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* decide(snapshot, expected_run_id, metadata, assignee) ->
 * {"outcome","contract","published_pr","assignee"} where outcome is
 *   "missing"  -- no tasks row (prepare_acceptance returns False)
 *   "skip"     -- falsy / "local-only" contract (returns None)
 *   "refuse"   -- status or expected_run_id mismatch (returns False)
 *   "prepare"  -- (snapshot, receipt) is returned; contract is the bound one.
 * snapshot is the tasks row as {"run_id","status","contract"}; null/absent for
 * no row.  expected_run_id null means "not checked" (None upstream). */
json_t *w24_kanbanprac_15da5e_decide(const json_t *snapshot, const json_t *expected_run_id,
                                     const json_t *metadata, const json_t *assignee);

/* pr_match(url) -> "<owner>/<repo>" when the URL is an exact GitHub PR URL
 * (re.fullmatch), else null. */
json_t *w24_kanbanprac_15da5e_pr_match(const json_t *url);

/* contract_ok(value) -> {"ok":true,"value":<validated>} or
 * {"ok":false,"error":<ValueError text>}; None / "local-only" normalize to
 * "local-only". */
json_t *w24_kanbanprac_15da5e_contract_ok(const json_t *value);

/* record_outcome(snapshot_matches, receipt) ->
 * {"result": <record_acceptance's return>, "detail": <last_failure_error|null>};
 * {"error":"KeyError"} when a required receipt key is absent. */
json_t *w24_kanbanprac_15da5e_record_outcome(const json_t *snapshot_matches,
                                             const json_t *receipt);

/* _PR / _REPO patterns and their flags, as an object. */
json_t *w24_kanbanprac_15da5e_constants(void);

#ifdef __cplusplus
}
#endif
#endif
