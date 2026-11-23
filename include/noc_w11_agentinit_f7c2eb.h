#ifndef NOC_W11_AGENTINIT_F7C2EB_H
#define NOC_W11_AGENTINIT_F7C2EB_H

/* C API for the agent/agent_init_fallback.py port (gate w11_agentinit_f7c2eb). */
#include "hermes_json.h"

/* _fallback_entries(fallback_model) -> json array of dicts */
json_t *w11_agentinit_f7c2eb_fallback_entries(const json_t *fallback_model);
/* display labels for a normalized chain: [f"{model} ({provider})", ...] */
json_t *w11_agentinit_f7c2eb_labels(const json_t *chain);
/* the human-readable chain message ("" for an empty chain) */
char *w11_agentinit_f7c2eb_message(const json_t *chain);

#endif /* NOC_W11_AGENTINIT_F7C2EB_H */
