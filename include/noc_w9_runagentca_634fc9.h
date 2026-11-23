#ifndef NOC_W9_RUNAGENTCA_634FC9_H
#define NOC_W9_RUNAGENTCA_634FC9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_runagentca_634fc9_first @ gateway/run_agent_cache.py:_first_agent */
struct json_t *w9_runagentca_634fc9_first(const struct json_t *in);

/* PoP: w9_runagentca_634fc9_tuple @ gateway/run_agent_cache.py:_tuple_agent */
struct json_t *w9_runagentca_634fc9_tuple(const struct json_t *in);

/* PoP: w9_runagentca_634fc9_config_sig @ gateway/run_agent_cache.py:GatewayAgentCacheMixin._agent_config_signature */
struct json_t *w9_runagentca_634fc9_config_sig(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_RUNAGENTCA_634FC9_H */
