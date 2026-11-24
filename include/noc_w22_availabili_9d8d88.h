#ifndef NOC_W22_AVAILABILI_9D8D88_H
#define NOC_W22_AVAILABILI_9D8D88_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_platform/resolver/availability.py (gate w22_availabili_9d8d88).
 *
 * `facts.os_family()`, `facts.gpu_class()` and the AppResolver locate/inspect
 * probes are host seams: the caller injects the two host facts and the located
 * app record, and the module's own state machine and version comparison run
 * here. The startup wizard and hermes_cli catalog stay upstream.
 */

/* PoP: w22_availabili_9d8d88_version_parts @ hermes_platform/resolver/availability.py:_version_parts
 * `[int(c) for c in version.split(".")]` when the whole string is 1-9 decimal
 * digits per component, else null. Components never exceed 9 digits, so they
 * fit exactly in an int64. */
json_t *w22_availabili_9d8d88_version_parts(const char *version);

/* PoP: w22_availabili_9d8d88_version_at_least @ hermes_platform/resolver/availability.py:version_at_least
 * Zero-padded component comparison; either side unparseable fails closed. */
int w22_availabili_9d8d88_version_at_least(const char *found, const char *minimum);

/* PoP: w22_availabili_9d8d88_offerable @ hermes_platform/resolver/availability.py:OFFERABLE
 * `state in {"available", "no_requirements"}` (unknown states are not offerable). */
int w22_availabili_9d8d88_offerable(const char *state);

/* PoP: w22_availabili_9d8d88_as_dict @ hermes_platform/resolver/availability.py:Availability.as_dict
 * All four keys, always, with null for an absent field. */
json_t *w22_availabili_9d8d88_as_dict(const char *state, const char *version,
                                     const char *path, const char *minimum);

/* PoP: w22_availabili_9d8d88_evaluate @ hermes_platform/resolver/availability.py:availability
 * The whole state machine. `entry` is {"requires_app", "min_version",
 * "required_gpu"}; `app` is the located definition for this OS
 * ({"found", "version_kind", "command", "candidates", "present"}) or null when
 * `app_for(os_family)` returned None. `present` rows are {"path", "version"}
 * where a null version means the inspect observation was not PRESENT. Returns
 * the Availability dict. */
json_t *w22_availabili_9d8d88_evaluate(const json_t *entry, const char *os_family,
                                       const char *gpu_class, const json_t *app);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_AVAILABILI_9D8D88_H */
