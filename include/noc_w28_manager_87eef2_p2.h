/*
 * noc_w28_manager_87eef2_p2.h - C surface for upstream
 * agent/checkpoints/manager.py (gate w28_manager_87eef2_p2).
 *
 * Gate 2: budget coercion, git-output parsing, project-manifest and
 * agent-write-ledger parsing, the ledger retention cap, and the /rollback
 * listing text.  Subprocess output and on-disk text arrive as fixture strings.
 */
#ifndef NOC_W28_MANAGER_87EEF2_P2_H
#define NOC_W28_MANAGER_87EEF2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CheckpointManager.__init__ -> {enabled, max_snapshots, max_total_size_mb,
 * max_file_size_mb} after the int()/max() clamps. */
json_t *w28_manager_87eef2_p2_coerce_init(const json_t *cfg);

/* _parse_shortstat -> {files_changed, insertions, deletions}. */
json_t *w28_manager_87eef2_p2_parse_shortstat(const char *stat_line);

/* list_checkpoints: {has_head, log, stats:{hash:{ok,out}}} -> entries. */
json_t *w28_manager_87eef2_p2_list_checkpoints(const json_t *req);

/* _load_ledger: manifest text -> object ({} when unusable). */
json_t *w28_manager_87eef2_p2_ledger_load(const char *text);

/* _save_ledger cap: newest 2000 entries, stable by "ts" descending. */
json_t *w28_manager_87eef2_p2_ledger_trim(const json_t *ledger);

/* _list_projects: [{hash,text},...] -> parsed metas with "_hash". */
json_t *w28_manager_87eef2_p2_list_projects(const json_t *entries);

/* format_checkpoint_list(checkpoints, directory). */
json_t *w28_manager_87eef2_p2_format_list(const json_t *checkpoints, const char *directory);

#ifdef __cplusplus
}
#endif
#endif
