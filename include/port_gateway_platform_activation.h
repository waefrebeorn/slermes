#ifndef PORT_GATEWAY_PLATFORM_ACTIVATION_H
#define PORT_GATEWAY_PLATFORM_ACTIVATION_H

#include "hermes_json.h"

/* Pure core of Python: gateway/platform_activation.py.
 * The file/env/pool scanners and the registry iterator are runtime and NOT
 * ported; every fact they would read arrives as a parameter. */

/* _yaml_block_names: the str keys of every dict-valued mapping under
 * yaml itself, yaml[platforms], yaml[gateway], yaml[gateway][platforms]. */
json_t *pa_yaml_block_names(const char *yaml_cfg_json);

/* _matches(name, candidates): any c lowers to ==name.lower() or
 * ==name.lower()+'_'... (a candidate whose lower starts with name.lower()+'_'). */
int pa_matches(const char *name, const json_t *candidates);

/* activation_predicate().has_evidence(name), with env/pool/json evidence
 * already resolved to sets (the closure's runtime reads). */
int pa_has_activation_evidence(const char *name,
                               const json_t *blocks, const json_t *pools,
                               const json_t *upper_env, const json_t *declared);

#endif /* PORT_GATEWAY_PLATFORM_ACTIVATION_H */
