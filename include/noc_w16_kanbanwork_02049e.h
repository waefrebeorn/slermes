#ifndef NOC_W16_KANBANWORK_02049E_H
#define NOC_W16_KANBANWORK_02049E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/kanban_worker_env.py (gate w16_kanbanwork_02049e). */

/* worker_client_env(env, deployment_names): the host-plumbing allowlist filter.
 * `deployment_names` is injected from the fixture (deployment_env_names seam);
 * the launch-profile strip is the oracle's injected identity seam. */
json_t *w16_kanbanwork_02049e_worker_client_env(const json_t *env,
                                                const json_t *deployment_names);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_KANBANWORK_02049E_H */
