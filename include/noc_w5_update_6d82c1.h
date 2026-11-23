#ifndef NOC_W5_UPDATE_6D82C1_H
#define NOC_W5_UPDATE_6D82C1_H
#include "hermes_json.h"

/* C home for upstream Hermes pm/update.py (gate w5_update_6d82c1).
 * The four pure helpers called out in the module (version parsing, minor
 * selection and per-target resolution) are ported. All network/index helpers
 * and the real re-pin install stay in Python. */

/* PoP: w5_update_6d82c1_version_key @ pm/update.py:version_key
 * JSON array of [0, <int>] / [1, <str>] pairs, matching Python's tuple key. */
json_t *w5_update_6d82c1_version_key(const char *version);

/* PoP: w5_update_6d82c1_minor_of @ pm/update.py:minor_of
 * JSON [major, minor] or null. */
json_t *w5_update_6d82c1_minor_of(const char *version);

/* PoP: w5_update_6d82c1_best_in_minor @ pm/update.py:best_in_minor
 * versions is a JSON array of strings; returns the highest version whose
 * major.minor matches, or null. */
json_t *w5_update_6d82c1_best_in_minor(json_t *versions, int major, int minor);

/* PoP: w5_update_6d82c1_resolve_best @ pm/update.py:resolve_best
 * latest_by_target is a JSON object mapping target -> [versions] (newest
 * first). locked_present=0 models locked=None. Returns the Resolved record as
 * a JSON object with name/locked/style/version/per_target/reason/
 * artifact_updates/changed. */
json_t *w5_update_6d82c1_resolve_best(const char *name,
                                      json_t *latest_by_target,
                                      const char *locked, int locked_present,
                                      const char *style);

#endif /* NOC_W5_UPDATE_6D82C1_H */
