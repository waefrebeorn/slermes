/*
 * noc_w21_managerpro_68c845.h -- public C API for the port of upstream Hermes
 *   agent/checkpoints/manager_profile_rename.py (gate w21_managerpro_68c845).
 *
 * Ported: the ledger rekey of _rebase_ledger_paths (Windows PurePath
 * semantics: case-insensitive prefix match, "." collapsed, ".." kept,
 * backslash output) and the whole scan/skip/rekey ladder of
 * migrate_profile_checkpoint_projects. The git store, the project list and
 * `Path.is_dir()` are the seams and arrive as `world`.
 */
#ifndef NOC_W21_MANAGERPRO_68C845_H
#define NOC_W21_MANAGERPRO_68C845_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _rebase_ledger_paths(ledger, old_workdir, new_workdir): absolute ledger keys
 * under old_workdir move under new_workdir, everything else is kept as-is.
 * Insertion order is Python dict order (a re-hit keeps its first position). */
json_t *w21_managerpro_68c845_rebase_ledger_paths(const json_t *ledger,
                                                  const char *old_workdir,
                                                  const char *new_workdir);

/* migrate_profile_checkpoint_projects(...) counters. `world`:
 *   old_root, new_root   -- the two profile dirs (already canonical)
 *   has_head             -- cm._store_has_head(store)
 *   git                  -- shutil.which("git") is not None
 *   projects             -- [{"workdir":str, "exists":bool, "fail":bool}]
 *                           (exists = new_workdir.is_dir(), fail = the rekey raised)
 * Returns {"scanned":n,"migrated":n,"errors":n}. */
json_t *w21_managerpro_68c845_migrate_plan(const json_t *world);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_managerpro_68c845_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_MANAGERPRO_68C845_H */
