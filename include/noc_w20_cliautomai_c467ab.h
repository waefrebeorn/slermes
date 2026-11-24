#ifndef NOC_W20_CLIAUTOMAI_C467AB_H
#define NOC_W20_CLIAUTOMAI_C467AB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/cli_auto_maintenance.py (gate w20_cliautomai_c467ab).
 *
 * The startup auto-maintenance pass: two latched one-time repairs, then the
 * configured auto-archive and auto-prune/vacuum, with the session-db handle
 * supplied as data (a scripted stub) instead of a live sqlite connection.
 */

/* PoP: w20_cliautomai_c467ab_run @ hermes_cli/cli_auto_maintenance.py:_run_state_db_auto_maintenance
 * `db` is `null` (session_db is None -> no-op) or
 *   {"meta": {key: value},                     get_meta / initial latch values
 *    "repairs": {key: {"count": n} | {"__raise__": {"type","message"}}},
 *    "calls": {"maybe_auto_archive"|"maybe_auto_prune_and_vacuum": {"__raise__": ...}}}
 * `config` is what load_config() returns
 *   ({"sessions": {...}}), or {"__raise__": {"type","message"}}.
 * `home` is get_hermes_home() (NULL -> "").
 * Returns {"events": [...]} in call order:
 *   {"call": "get_meta", "args": [key]}
 *   {"call": "prune_empty_ghost_sessions", "kwargs": {"sessions_dir": "..."}}
 *   {"call": "finalize_orphaned_compression_sessions", "kwargs": {}}
 *   {"call": "set_meta", "args": [key, "1"]}
 *   {"call": "load_config", "kwargs": {}}
 *   {"call": "maybe_auto_archive", "kwargs": {"idle_days": f, "min_interval_hours": i}}
 *   {"call": "maybe_auto_prune_and_vacuum", "kwargs": {...}}
 *   {"log": "warning"|"info"|"debug", "msg": "..."}   (the formatted record)
 * Never raises: the module's own excepts swallow every failure into a log. */
json_t *w20_cliautomai_c467ab_run(const json_t *db, const json_t *config, const char *home);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CLIAUTOMAI_C467AB_H */
