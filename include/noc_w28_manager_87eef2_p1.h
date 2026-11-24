/*
 * noc_w28_manager_87eef2_p1.h - C surface for upstream
 * agent/checkpoints/manager.py (gate w28_manager_87eef2_p1).
 *
 * Gate 1: input validation, checkpoint identifiers, path derivation and the
 * git argument shaping that keeps the shared shadow store out of a snapshot.
 * Every filesystem seam (Path.resolve, exists) is a fixture input; the strings
 * here are PurePosixPath forms.
 */
#ifndef NOC_W28_MANAGER_87EEF2_P1_H
#define NOC_W28_MANAGER_87EEF2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _validate_commit_hash: null when valid, else the error string. */
json_t *w28_manager_87eef2_p1_validate_commit_hash(const char *commit_hash);

/* _ref_name: "refs/hermes/<dir_hash>". */
json_t *w28_manager_87eef2_p1_ref_name(const char *dir_hash);

/* _gitlink_paths: records of `git ls-tree -r -z` -> gitlink paths. */
json_t *w28_manager_87eef2_p1_gitlink_paths(const json_t *records);

/* _uncaptured_note: nested-repo paths -> the checkpoint-message suffix. */
json_t *w28_manager_87eef2_p1_uncaptured_note(const json_t *nested_repos);

/* _project_hash: sha256(abs_path)[:16]. */
json_t *w28_manager_87eef2_p1_project_hash(const char *abs_path);

/* _store_path / _index_path / _ledger_path / _project_meta_path. */
json_t *w28_manager_87eef2_p1_store_path(const char *base);
json_t *w28_manager_87eef2_p1_index_path(const char *store, const char *dir_hash);
json_t *w28_manager_87eef2_p1_ledger_path(const char *store, const char *dir_hash);
json_t *w28_manager_87eef2_p1_meta_path(const char *store, const char *dir_hash);

/* _store_pathspecs / _exclude_store_args / _stage_all_args. */
json_t *w28_manager_87eef2_p1_store_pathspecs(const char *base, const char *working_dir);
json_t *w28_manager_87eef2_p1_exclude_store_args(const char *base, const char *working_dir);
json_t *w28_manager_87eef2_p1_stage_all_args(const char *base, const char *working_dir);

#ifdef __cplusplus
}
#endif
#endif
