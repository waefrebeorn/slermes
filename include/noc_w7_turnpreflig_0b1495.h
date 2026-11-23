#ifndef NOC_W7_TURNPREFLIG_0B1495_H
#define NOC_W7_TURNPREFLIG_0B1495_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: turnpreflig_run_preflight @ agent/turn_preflight.py:run_preflight_compression
 *
 * Faithful C11 mirror of the pre-API compression gate's control flow. The
 * live function's agent/compressor collaborators and the lazy helpers it
 * imports from agent.conversation_loop et al are injected deterministically by
 * the oracle; every branch below corresponds 1:1 with the upstream decision
 * tree (eligibility, defer/cooldown/lock gates, window grow, timeout, handoff
 * skip, provider-overflow fail-closed). `in` carries the injected flags; the
 * returned object carries the rebound loop locals plus the action/result. */
json_t *turnpreflig_run_preflight(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
