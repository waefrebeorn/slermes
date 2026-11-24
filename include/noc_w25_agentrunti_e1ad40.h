/*
 * noc_w25_agentrunti_e1ad40.h - C surface for upstream
 * agent/agent_runtime_helpers_dead_connections.py (gate w25_agentrunti_e1ad40).
 *
 * The pre-turn sweep over an agent's OpenAI client pools.  The sockets, the
 * httpcore pool walk and the client teardown hooks are seams; what is ported is
 * the decision they feed: which pools are inspected, which are reported dead,
 * the advisory that names them, and which ownership-aware teardown each one
 * gets (primary -> rebuild, request cache -> cache hook).
 */
#ifndef NOC_W25_AGENTRUNTI_E1AD40_H
#define NOC_W25_AGENTRUNTI_E1AD40_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* probe result of one pool socket, as the recv peek classified it:
 * "eof" (b"" -> dead), "data" (bytes -> healthy), "blocking"
 * (BlockingIOError -> healthy), "oserror" / "setblocking_error" (dead).
 * Any other value -> null (not a probe outcome). */
json_t *w25_agentrunti_e1ad40_socket_probe(const json_t *kind);

/* plan: {
 *   primary:        <bool>,          # agent.client is set
 *   request_client: <bool>,          # _request_client_cache["client"] is set
 *   same:           <bool>,          # that client IS agent.client
 *   dead:           {"primary": n, "request": m},
 *   probe_error:    ["primary"|"request", ...],
 *   cleanup_error:  ["primary"|"request", ...]
 * } -> {"cleaned": bool, "actions": [...], "logs": [[level, message], ...]} */
json_t *w25_agentrunti_e1ad40_cleanup(const json_t *plan);

/* Literal contract: logger name and the reason tag every teardown carries. */
json_t *w25_agentrunti_e1ad40_constants(void);

#ifdef __cplusplus
}
#endif
#endif
