#ifndef NOC_W21_KANBANDBDE_905B79_H
#define NOC_W21_KANBANDBDE_905B79_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/kanban_db_default_assignee.py (gate
 * w21_kanbandbde_905b79).
 *
 * The module is one database write plus the bookkeeping that decides what a
 * dispatch tick does with an unassigned ready card. The sqlite connection, the
 * `write_txn` context manager and `_append_event` are the seam: the caller
 * reports the UPDATE's rowcount (or that it raised) and the post-failure
 * `SELECT assignee` row, and this port carries every branch of the decision --
 * dry-run success, the row genuinely updated, the concurrent-assign rowcount
 * miss, the exception path, the deferral read, and both result buckets.
 */

/* PoP: w21_kanbandbde_905b79_apply @ hermes_cli/kanban_db_default_assignee.py:apply_default_assignee
 * persist `kanban.default_assignee` on an unassigned row.
 *   dry_run  -- report without writing (True before touching the row)
 *   rowcount -- updated.rowcount (ignored when dry_run or raises)
 *   raises   -- the write_txn/execute raised (the except branch -> False)
 * Answers {"applied": bool, "events": [{"task_id","kind","payload"}]}. */
json_t *w21_kanbandbde_905b79_apply(const char *task_id, const char *assignee,
                                    int dry_run, int rowcount, int raises);

/* PoP: w21_kanbandbde_905b79_fill @ hermes_cli/kanban_db_default_assignee.py:fill_default_assignee
 * assignee to dispatch an unassigned ready card as, or null to skip this tick.
 *   current_row -- {"assignee": ...} the post-failure SELECT returned, or null
 *                  when fetchone() gave None
 *   result      -- {"skipped_unassigned": [...], "auto_assigned_default": [...]}
 * Answers {"assignee": <default|null>, "skipped_unassigned": [...],
 *          "auto_assigned_default": [...], "events": [...]}. */
json_t *w21_kanbandbde_905b79_fill(const char *task_id, const json_t *default_assignee,
                                   int dry_run, int rowcount, int raises,
                                   const json_t *current_row, const json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_KANBANDBDE_905B79_H */
