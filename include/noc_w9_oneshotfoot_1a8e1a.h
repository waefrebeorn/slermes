#ifndef NOC_W9_ONESHOTFOOT_1A8E1A_H
#define NOC_W9_ONESHOTFOOT_1A8E1A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* PoP: w9_oneshotfoot_1a8e1a_is_single_query_ex @ agent/oneshot_footprint.py:is_single_query_session
 * Explicit-seam port: policy_source == NULL means no owner-bound frozen policy,
 * in which case the env value decides (=="1"). Returns the one-shot decision. */
bool w9_oneshotfoot_1a8e1a_is_single_query_ex(const char *policy_source,
                                               const char *env_value);

/* PoP: w9_oneshotfoot_1a8e1a_agent_is_oneshot @ agent/oneshot_footprint.py:agent_is_oneshot
 * cached: -1 = no bool cached (compute), 0/1 = frozen bool. */
bool w9_oneshotfoot_1a8e1a_agent_is_oneshot(int cached, bool computed);

/* PoP: w9_oneshotfoot_1a8e1a_disabled_toolsets @ agent/oneshot_footprint.py:ONESHOT_DISABLED_TOOLSETS
 * Returns a JSON array of toolsets a one-shot never gets (caller frees). */
struct json_t *w9_oneshotfoot_1a8e1a_disabled_toolsets(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_ONESHOTFOOT_1A8E1A_H */
