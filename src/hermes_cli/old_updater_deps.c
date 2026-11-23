/*
 * old_updater_deps.c — real C11 port of the observable interface of
 * upstream Hermes hermes_cli/old_updater_deps.py (gate w15_oldupdater_cc42de).
 *
 * Each retired shim's entire behaviour is a call to
 * stop_historical_updater(hook, error); outside a historical update that
 * raises error(hook-message). This port returns the error class name and
 * the hook string so a differential oracle can compare exactly which hook
 * each shim selects. Shims that simply return None produce NULL.
 * PoP: w15_oldupdater_cc42de_capture_active_lazy_features @ hermes_cli/old_updater_deps.py:_capture_active_lazy_features
 * PoP: w15_oldupdater_cc42de_refresh_active_lazy_features @ hermes_cli/old_updater_deps.py:_refresh_active_lazy_features
 * PoP: w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies @ hermes_cli/old_updater_deps.py:_refresh_active_memory_provider_dependencies
 * PoP: w15_oldupdater_cc42de_npm_lockfile_changed @ hermes_cli/old_updater_deps.py:_npm_lockfile_changed
 * PoP: w15_oldupdater_cc42de_update_node_dependencies @ hermes_cli/old_updater_deps.py:_update_node_dependencies
 * PoP: w15_oldupdater_cc42de_rebuild_desktop_after_update @ hermes_cli/old_updater_deps.py:_rebuild_desktop_after_update
 * PoP: w15_oldupdater_cc42de_write_update_incomplete_marker @ hermes_cli/old_updater_deps.py:_write_update_incomplete_marker
 * PoP: w15_oldupdater_cc42de_path_uid @ hermes_cli/old_updater_deps.py:_path_uid
 * PoP: w15_oldupdater_cc42de_leftover_pausable_gateway_pids @ hermes_cli/old_updater_deps.py:_leftover_pausable_gateway_pids
 * PoP: w15_oldupdater_cc42de_ledger_manual_serve_holders @ hermes_cli/old_updater_deps.py:_ledger_manual_serve_holders
 * PoP: w15_oldupdater_cc42de_relaunch_stopped_serves @ hermes_cli/old_updater_deps.py:_relaunch_stopped_serves
 * PoP: w15_oldupdater_cc42de_orphaned_desktop_backend_pids @ hermes_cli/old_updater_deps.py:_orphaned_desktop_backend_pids
 * PoP: w15_oldupdater_cc42de_ledger_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_ledger_reapable_backend_pids
 * PoP: w15_oldupdater_cc42de_handoff_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_handoff_reapable_backend_pids
 * PoP: w15_oldupdater_cc42de_stop_process_trees @ hermes_cli/old_updater_deps.py:_stop_process_trees
 */
#include "noc_w15_oldupdater_cc42de.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *w15_oldupdater_cc42de_compose(const char *err, const char *hook) {
    size_t n = strlen(err) + strlen(hook) + 2;
    char *s = (char *)malloc(n);
    if (s) snprintf(s, n, "%s|%s", err, hook);
    return s;
}

/* PoP: w15_oldupdater_cc42de_capture_active_lazy_features @ hermes_cli/old_updater_deps.py:_capture_active_lazy_features */
char *w15_oldupdater_cc42de_capture_active_lazy_features(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._capture_active_lazy_features");
}

/* PoP: w15_oldupdater_cc42de_refresh_active_lazy_features @ hermes_cli/old_updater_deps.py:_refresh_active_lazy_features */
char *w15_oldupdater_cc42de_refresh_active_lazy_features(void) {
    return w15_oldupdater_cc42de_compose("ImportError", "hermes_cli.old_updater_deps._refresh_active_lazy_features");
}

/* PoP: w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies @ hermes_cli/old_updater_deps.py:_refresh_active_memory_provider_dependencies */
char *w15_oldupdater_cc42de_refresh_active_memory_provider_dependencies(void) {
    return w15_oldupdater_cc42de_compose("ImportError", "hermes_cli.old_updater_deps._refresh_active_memory_provider_dependencies");
}

/* PoP: w15_oldupdater_cc42de_npm_lockfile_changed @ hermes_cli/old_updater_deps.py:_npm_lockfile_changed */
char *w15_oldupdater_cc42de_npm_lockfile_changed(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._npm_lockfile_changed");
}

/* PoP: w15_oldupdater_cc42de_update_node_dependencies @ hermes_cli/old_updater_deps.py:_update_node_dependencies */
char *w15_oldupdater_cc42de_update_node_dependencies(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._update_node_dependencies");
}

/* PoP: w15_oldupdater_cc42de_rebuild_desktop_after_update @ hermes_cli/old_updater_deps.py:_rebuild_desktop_after_update */
char *w15_oldupdater_cc42de_rebuild_desktop_after_update(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._rebuild_desktop_after_update");
}

/* PoP: w15_oldupdater_cc42de_write_update_incomplete_marker @ hermes_cli/old_updater_deps.py:_write_update_incomplete_marker */
char *w15_oldupdater_cc42de_write_update_incomplete_marker(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._write_update_incomplete_marker");
}

/* PoP: w15_oldupdater_cc42de_path_uid @ hermes_cli/old_updater_deps.py:_path_uid */
char *w15_oldupdater_cc42de_path_uid(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._path_uid");
}

/* PoP: w15_oldupdater_cc42de_leftover_pausable_gateway_pids @ hermes_cli/old_updater_deps.py:_leftover_pausable_gateway_pids */
char *w15_oldupdater_cc42de_leftover_pausable_gateway_pids(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._leftover_pausable_gateway_pids");
}

/* PoP: w15_oldupdater_cc42de_ledger_manual_serve_holders @ hermes_cli/old_updater_deps.py:_ledger_manual_serve_holders */
char *w15_oldupdater_cc42de_ledger_manual_serve_holders(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._ledger_manual_serve_holders");
}

/* PoP: w15_oldupdater_cc42de_relaunch_stopped_serves @ hermes_cli/old_updater_deps.py:_relaunch_stopped_serves */
char *w15_oldupdater_cc42de_relaunch_stopped_serves(int pending) {
    if (!pending) return NULL;
    return dupstr("delegate|relaunch_stopped_serves");
}

/* PoP: w15_oldupdater_cc42de_orphaned_desktop_backend_pids @ hermes_cli/old_updater_deps.py:_orphaned_desktop_backend_pids */
char *w15_oldupdater_cc42de_orphaned_desktop_backend_pids(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._orphaned_desktop_backend_pids");
}

/* PoP: w15_oldupdater_cc42de_ledger_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_ledger_reapable_backend_pids */
char *w15_oldupdater_cc42de_ledger_reapable_backend_pids(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._ledger_reapable_backend_pids");
}

/* PoP: w15_oldupdater_cc42de_handoff_reapable_backend_pids @ hermes_cli/old_updater_deps.py:_handoff_reapable_backend_pids */
char *w15_oldupdater_cc42de_handoff_reapable_backend_pids(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._handoff_reapable_backend_pids");
}

/* PoP: w15_oldupdater_cc42de_stop_process_trees @ hermes_cli/old_updater_deps.py:_stop_process_trees */
char *w15_oldupdater_cc42de_stop_process_trees(void) {
    return w15_oldupdater_cc42de_compose("RetiredUpdaterHook", "hermes_cli.old_updater_deps._stop_process_trees");
}

