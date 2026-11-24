#ifndef NOC_W22_UPDATECMDV_CE00F8_H
#define NOC_W22_UPDATECMDV_CE00F8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/update_cmd_validation.py (gate w22_updatecmdv_ce00f8).
 *
 * `_critical_module_import_failures` runs an import probe in a boot-selected
 * subprocess and reads the failures it reports after a random marker. The
 * subprocess is the seam: its stdout and return code are injected here, and
 * everything the module decides about that output is ported:
 *
 *   - the marker is `__HERMES_IMPORT_HEALTH_<32 lowercase hex>__`; the payload
 *     is everything after the last marker occurrence.
 *   - a missing marker is `ProbeTerminated` carrying the exit code.
 *   - the payload must decode (strictly, like json.loads) to a list of
 *     [module, kind, detail] triples of strings; anything else is
 *     `MalformedPayload`, and duplicate modules keep the first position with
 *     the last tuple.
 *   - `_validate_critical_modules_import` reports the first failure's module
 *     and detail.
 */

/* PoP: w22_updatecmdv_ce00f8_probe_failure @ hermes_cli/update_cmd_validation.py:_probe_failure
 * {"critical-module probe": [kind, detail]} -- the failure row for the probe
 * itself, as opposed to a module it imported. */
json_t *w22_updatecmdv_ce00f8_probe_failure(const char *kind, const char *detail);

/* PoP: w22_updatecmdv_ce00f8_failures @ hermes_cli/update_cmd_validation.py:_critical_module_import_failures
 * {"<module>": [kind, detail]} from the probe's stdout (NULL = empty stdout) and
 * its return code (int64 decimal string; NULL reads as 0). */
json_t *w22_updatecmdv_ce00f8_failures(const char *stdout_text, const char *returncode);

/* PoP: w22_updatecmdv_ce00f8_validate @ hermes_cli/update_cmd_validation.py:_validate_critical_modules_import
 * [ok, module-or-null, message-or-null]: the first critical-module import
 * failure, if any. */
json_t *w22_updatecmdv_ce00f8_validate(const char *stdout_text, const char *returncode);

/* PoP: w22_updatecmdv_ce00f8_probe_timeout @ hermes_cli/update_cmd_validation.py:_critical_module_import_failures
 * The `subprocess.TimeoutExpired` row: {"critical-module probe":
 * ["TimeoutExpired", "timed out before reporting import health"]}. */
json_t *w22_updatecmdv_ce00f8_probe_timeout(void);

/* PoP: w22_updatecmdv_ce00f8_modules @ hermes_cli/update_cmd_validation.py:_UPDATE_CRITICAL_MODULES
 * Modules imported on every startup; the probe order is this order. */
extern const char *const W22_UPDATECMDV_CE00F8_MODULES[5];

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_UPDATECMDV_CE00F8_H */
