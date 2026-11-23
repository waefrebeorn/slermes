#ifndef NOC_W14_PLATFORMAC_7882B1_H
#define NOC_W14_PLATFORMAC_7882B1_H

#include "hermes_json.h"

/* C port of the pure activation-evidence helpers in
 * gateway/platform_activation.py (gate w14_platformac_7882b1).
 *
 * The live module scans the process env, .env files and the credential pool;
 * those results are injected as explicit JSON string arrays here so the
 * decision logic replays deterministically (the Windows setenv("") pitfall).
 *
 * yaml_block_names -> new json_t* array of sorted unique block names.
 * matches / has_evidence -> new json_t* bool.
 * has_evidence mirrors activation_predicate's inner `has_evidence` with
 * `blocks` already = yaml blocks | platform_names | legacy gateway.json names.
 */
json_t *w14_platformac_7882b1_yaml_block_names(json_t *yaml_cfg);
json_t *w14_platformac_7882b1_matches(const char *name, json_t *candidates);
json_t *w14_platformac_7882b1_has_evidence(const char *name, json_t *blocks,
                                           json_t *pools, json_t *env,
                                           json_t *declared);

#endif /* NOC_W14_PLATFORMAC_7882B1_H */
