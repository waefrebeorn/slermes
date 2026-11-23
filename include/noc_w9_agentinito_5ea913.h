#ifndef NOC_W9_AGENTINITO_5EA913_H
#define NOC_W9_AGENTINITO_5EA913_H

#include <hermes_json.h>

/* C11 ports of the local-server (Ollama) num_ctx init helpers in
 * agent/agent_init_ollama.py. Every symbol is prefixed w9_agentinito_5ea913_
 * and takes one borrowed json_t *args object, returning a NEW json_t *.
 * Detection / local-server results are injected via the args object. */
json_t *w9_agentinito_5ea913_configure_num_ctx(json_t *a);
json_t *w9_agentinito_5ea913_clamp_compressor(json_t *a);

#endif /* NOC_W9_AGENTINITO_5EA913_H */
