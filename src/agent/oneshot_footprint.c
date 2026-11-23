/*
 * oneshot_footprint.c - real C11 port of upstream Hermes agent/oneshot_footprint.py.
 *
 * PoP: w9_oneshotfoot_1a8e1a_is_single_query_ex @ agent/oneshot_footprint.py:is_single_query_session
 * PoP: w9_oneshotfoot_1a8e1a_agent_is_oneshot    @ agent/oneshot_footprint.py:agent_is_oneshot
 * PoP: w9_oneshotfoot_1a8e1a_disabled_toolsets  @ agent/oneshot_footprint.py:ONESHOT_DISABLED_TOOLSETS
 *
 * The one-shot decision reads session env / frozen policy.  Per the port-kit
 * env pitfall, the decision is an explicit _ex seam taking the policy source
 * and env value from the fixture instead of mutating the CRT environment
 * (Windows setenv("") unsets; Python keeps it).
 */
#include "hermes_json.h"
#include "noc_w9_oneshotfoot_1a8e1a.h"
#include <string.h>

bool w9_oneshotfoot_1a8e1a_is_single_query_ex(const char *policy_source,
                                               const char *env_value)
{
    /* A gateway-owned session's frozen policy wins; source == "oneshot". */
    if (policy_source)
        return strcmp(policy_source, "oneshot") == 0;
    /* Otherwise str(get_session_env("HERMES_SINGLE_QUERY_SESSION", "") or "") == "1". */
    return env_value && strcmp(env_value, "1") == 0;
}

bool w9_oneshotfoot_1a8e1a_agent_is_oneshot(int cached, bool computed)
{
    /* cached: -1 = attribute absent / not a bool -> compute; 0/1 = frozen bool. */
    return cached < 0 ? computed : (cached != 0);
}

struct json_t *w9_oneshotfoot_1a8e1a_disabled_toolsets(void)
{
    struct json_t *arr = json_array();
    json_append(arr, json_string("clarify"));
    return arr;
}
