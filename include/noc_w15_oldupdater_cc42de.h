#ifndef NOC_W15_OLDUPDATER_CC42DE_H
#define NOC_W15_OLDUPDATER_CC42DE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the retired-updater shims in hermes_cli/old_updater_deps.py (gate w15_oldupdater_cc42de).
 * Outside a historical update, every 'none' shim returns NULL (Python None).
 * Every 'raise' shim returns 'ErrorName|hook' — the class name and hook string
 * passed to stop_historical_updater. Caller frees returned strings. */

/* PoP: w15_oldupdater_cc42de_capture_active_lazy_features @ hermes_cli/old_updater_deps.py:_capture_active_lazy_features */
char *w15_oldupdater_cc42de_capture_active_lazy_features(void);

/* PoP: w15_oldupdater_cc42de_refresh_active_lazy_features @ hermes_cli/old_updater_deps.py:_refresh_active_lazy_features */
char *w15_oldupdater_cc42de_refresh_active_lazy_features(void);

/* PoP: w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies @ hermes_cli/old_updater_deps.py:_refresh_active_memory_provider_dependencies */
char *w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies(void);

/* PoP: w15_oldupdater_cc42de_npm_lockfile_changed @ hermes_cli/old_updater_deps.py:_npm_lockfile_changed */
char *w15_oldupdater_cc42de_npm_lockfile_changed(void);

/* PoP: w15_oldupdater_cc42de_update_node_dependencies @ hermes_cli/old_updater_deps.py:_update_node_dependencies */
char *w15_oldupdater_cc42de_update_node_dependencies(void);

/* PoP: w15_oldupdater_cc42de_rebuild_desktop_after_update @ hermes_cli/old_updater_deps.py:_rebuild_desktop_after_update */
char *w15_oldupdater_cc42de_rebuild_desktop_after_update(void);

/* PoP: w15_oldupdater_cc42de_write_update_incomplete_marker @ hermes_cli/old_updater_deps.py:_write_update_incomplete_marker */
char *w15_oldupdater_cc42de_write_update_incomplete_marker(void);

/* PoP: w15_oldupdater_cc42de_path_uid @ hermes_cli/old_updater_deps.py:_path_uid */
char *w15_oldupdater_cc42de_path_uid(void);

/* PoP: w15_oldupdater_cc42de_leftover_pausable_gateway_pids @ hermes_cli/old_updater_deps.py:_leftover_pausable_gateway_pids */
char *w15_oldupdater_cc42de_leftover_pausable_gateway_pids(void);

/* PoP: w15_oldupdater_cc42de_ledger_manual_serve_holders @ hermes_cli/old_updater_deps.py:_ledger_manual_serve_holders */
char *w15_oldupdater_cc42de_ledger_manual_serve_holders(void);

/* PoP: w15_oldupdater_cc42de_relaunch_stopped_serves @ hermes_cli/old_updater_deps.py:_relaunch_stopped_serves */
char *w15_oldupdater_cc42de_relaunch_stopped_serves(int pending);

/* PoP: w15_oldupdater_cc42de_orphaned_desktop_backend_pids @ hermes_cli/old_updater_deps.py:_orphaned_desktop_backend_pids */
char *w15_oldupdater_cc42de_orphaned_desktop_backend_pids(void);

/* PoP: w15_oldupdater_cc42de_ledger_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_ledger_reapable_backend_pids */
char *w15_oldupdater_cc42de_ledger_reapable_backend_pids(void);

/* PoP: w15_oldupdater_cc42de_handoff_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_handoff_reapable_backend_pids */
char *w15_oldupdater_cc42de_handoff_reapable_backend_pids(void);

/* PoP: w15_oldupdater_cc42de_stop_process_trees @ hermes_cli/old_updater_deps.py:_stop_process_trees */
char *w15_oldupdater_cc42de_stop_process_trees(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_OLDUPDATER_CC42DE_H */
