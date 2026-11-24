/*
 * noc_w28_manager_87eef2_p3.h - C surface for upstream
 * agent/checkpoints/manager.py (gate w28_manager_87eef2_p3).
 *
 * Gate 3: the restore and diff decisions — which changed files a safe restore
 * may touch, what a diff attempt reports, how session_diff picks its baseline,
 * and the per-file size cap.  Git results, hashes and stat sizes arrive as
 * fixture inputs.
 */
#ifndef NOC_W28_MANAGER_87EEF2_P3_H
#define NOC_W28_MANAGER_87EEF2_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _safe_restore_plan: {commit, dir, names, ledger, current, has_head, ok, err}
 * -> {success, restore, skipped[, ledger_empty]} or {success:false, error}. */
json_t *w28_manager_87eef2_p3_safe_restore_plan(const json_t *req);

/* _diff: {commit, has_head, cat_ok, stat_ok, stat, diff_ok, diff}
 * -> {success, stat, diff} or {success:false, error}. */
json_t *w28_manager_87eef2_p3_diff_result(const json_t *req);

/* session_diff: {checkpoints, diff} -> the diff result with baseline/empty. */
json_t *w28_manager_87eef2_p3_session_diff(const json_t *req);

/* _exceeds_size_cap: {size, cap_mb, stat_ok} -> bool. */
json_t *w28_manager_87eef2_p3_exceeds_size_cap(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
