#ifndef NOC_W20_GATEWAYRES_A0C591_H
#define NOC_W20_GATEWAYRES_A0C591_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_restart_watcher.py (gate w20_gatewayres_a0c591).
 *
 * The module renders the ``python -c`` program the updater detaches to respawn
 * the gateway after the old one exits. Both of its products are pure strings,
 * so both are ported here; the live start-time probe is the injected seam.
 */

/* Number of .format fields in the template (_SOURCE): project_root_literal,
 * respawn_cwd_literal, respawn_env_literal, watcher_timeout_literal,
 * start_time_literal. */
#define W20_GATEWAYRES_A0C591_FIELD_COUNT 5

/* PoP: w20_gatewayres_a0c591_source_template @ hermes_cli/gateway_restart_watcher.py:_SOURCE
 * The module-level dedented template, byte for byte, still carrying its
 * ``{name}`` fields and str.format's doubled braces. Malloc'd; caller frees. */
char *w20_gatewayres_a0c591_source_template(void);

/* PoP: w20_gatewayres_a0c591_watcher_source @ hermes_cli/gateway_restart_watcher.py:watcher_source
 * ``_SOURCE.format(...)``: every field replaced by its json.dumps/repr literal
 * and the doubled braces collapsed. ``respawn_env_overlay`` is the parsed
 * mapping (NULL reads as {}), ``timeout_s`` is json.dumps'd as a float, and
 * ``start_time`` is the decimal string for process_start_time_stdlib(old_pid)
 * or NULL for Python's None. ``old_pid`` never appears in the source: only its
 * start time does, through ``start_time``. Malloc'd; caller frees; NULL only on
 * allocation failure. */
char *w20_gatewayres_a0c591_watcher_source(long long old_pid, const char *respawn_cwd,
                                           const json_t *respawn_env_overlay, double timeout_s,
                                           const char *project_root, const char *start_time);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_GATEWAYRES_A0C591_H */
