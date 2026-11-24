#ifndef NOC_W21_SESSIONSCM_34A9F9_H
#define NOC_W21_SESSIONSCM_34A9F9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/sessions_cmd_journal_mode.py (gate w21_sessionscm_34a9f9).
 *
 * `cmd_set_journal_mode` is a console command: its whole observable behaviour is
 * the lines it prints and the exit code it returns. The store admin, the
 * profile's store location, the config's journal mode and the sqlite handle are
 * seams; everything that decides *what to print* is local, so the port returns
 * {"code": int, "lines": [str, ...]} in print order.
 *
 * Request object (the args namespace + the seams, as one dict):
 *   mode            -- args.mode (rendered with str(); any scalar)
 *   db              -- args.db, a path string or null
 *   store_location  -- what store_location(profile_scope()) falls back to
 *   db_exists       -- bool: does the path exist (Path(db_path).exists())
 *   admin           -- "ok" | "unsupported" | "none": open_store_admin's result
 *   result          -- {"current": s, "after": s} from set_journal_mode
 *   error           -- PersistenceError text, which set_journal_mode raised
 *   configured      -- resolve_journal_mode(): config.yaml's current mode
 * Paths are taken as Path would print them on the host (a basename in the
 * fixtures, so no separator normalization is implied) and are used verbatim in
 * the messages.
 */

/* PoP: w21_sessionscm_34a9f9_run_set_journal_mode @ hermes_cli/sessions_cmd_journal_mode.py:cmd_set_journal_mode
 * Never NULL. code 0 when the mode was already set or the flip succeeded, 1 for
 * the no-file / no-database / PersistenceError refusals. */
json_t *w21_sessionscm_34a9f9_run_set_journal_mode(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_SESSIONSCM_34A9F9_H */
