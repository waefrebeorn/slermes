#ifndef NOC_W22_PROCESSREG_606E65_H
#define NOC_W22_PROCESSREG_606E65_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/process_registry_results.py (gate w22_processreg_606e65).
 *
 * The receipt directory (`get_hermes_home()/logs/process-results`), the wall
 * clock, the session env and the persistence lineage read are host seams: the
 * caller injects the directory listing, "now", the owner session id and the
 * lineage tips, and the module's retention, ownership and admission decisions
 * run here. Writing receipts (`atomic_json_write`, redaction) and constructing
 * live ProcessSession objects stay upstream.
 */

/* PoP: w22_processreg_606e65_field_names @ tools/process_registry_results.py:_RESULT_FIELDS
 * The 13 receipt fields, in declaration order. */
json_t *w22_processreg_606e65_field_names(void);

/* PoP: w22_processreg_606e65_retention_seconds @ tools/process_registry_results.py:RESULT_RETENTION_SECONDS
 * `7 * 24 * 60 * 60`. */
long long w22_processreg_606e65_retention_seconds(void);

/* PoP: w22_processreg_606e65_max_retained @ tools/process_registry_results.py:MAX_RETAINED_RESULTS */
long long w22_processreg_606e65_max_retained(void);

/* PoP: w22_processreg_606e65_select_retained @ tools/process_registry_results.py:_result_paths
 * `_result_paths` over an injected listing. `rows` are {"name", "mtime"} with
 * mtime as a decimal string (epoch seconds); only `proc_*.json` names exist, as
 * with `directory.glob("proc_*.json")`. Every row older than
 * now - RESULT_RETENTION_SECONDS is pruned, the rest sort by (mtime, name)
 * descending and everything past the first MAX_RETAINED_RESULTS is pruned.
 * Returns {"retained": [names in kept order], "pruned": [names, sorted]}. */
json_t *w22_processreg_606e65_select_retained(const json_t *rows, const char *now);

/* PoP: w22_processreg_606e65_owns_result @ tools/process_registry_results.py:_owns_result
 * No parent -> not ours; owner == parent -> ours; else the lineage tip of the
 * parent must be the owner. `lineage` maps parent id -> tip id (the read seam). */
int w22_processreg_606e65_owns_result(const char *owner, const char *parent,
                                      const json_t *lineage);

/* PoP: w22_processreg_606e65_admissible @ tools/process_registry_results.py:load_completed_results
 * The per-file filter of a restore: no owner reads nothing; the file stem must
 * carry the prefix, the receipt id must equal the stem and be `proc_[A-Za-z0-9_]+`,
 * and the receipt must belong to the reading session. */
int w22_processreg_606e65_admissible(const char *prefix, const char *stem,
                                     const char *record_id, const char *owner,
                                     const char *parent, const json_t *lineage);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_PROCESSREG_606E65_H */
