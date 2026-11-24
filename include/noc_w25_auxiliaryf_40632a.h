/*
 * noc_w25_auxiliaryf_40632a.h - C surface for upstream
 * agent/auxiliary_fallback_recovery.py (gate w25_auxiliaryf_40632a).
 *
 * The parameter-rejection recovery a fallback candidate's request gets: the
 * same strip rungs the primary auxiliary path runs.  Sending, the ladder drive
 * loop and the provider's 400 are seams; what is ported is the value-only core:
 * ``reapply_repairs`` over a request and the strips that already ran, and the
 * route/max_tokens facts ``_parameter_ladder`` derives from the request.
 */
#ifndef NOC_W25_AUXILIARYF_40632A_H
#define NOC_W25_AUXILIARYF_40632A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of reasoning wire keys a thinking-off profile may forward. */
#define W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS 10
/* Number of efforts that mean "thinking off". */
#define W25_AUXILIARYF_40632A_DISABLED_EFFORTS 5

/* reapply_repairs(kwargs, repairs):
 * request: {"kwargs": {...}, "repairs": ["temperature" | "structured_output" |
 *           "reasoning_floor" | "reasoning" | "max_tokens", ...]} -> the
 * rebuilt request, each strip's ``or kwargs`` applied in order. */
json_t *w25_auxiliaryf_40632a_reapply_repairs(const json_t *request);

/* _parameter_ladder's route facts:
 * request: {"kwargs": {...}, "tag": <str>, "task": <str|null>,
 *           "provider": <str>, "base_url": <str>, "client_base_url": <str>}
 * -> {"max_tokens", "final_model", "base_info", "async_mode", "tag", "task",
 *     "resolved_provider"}. */
json_t *w25_auxiliaryf_40632a_ladder_route(const json_t *request);

/* The strip tables: profile reasoning keys, disabled efforts, floor effort. */
json_t *w25_auxiliaryf_40632a_constants(void);

#ifdef __cplusplus
}
#endif
#endif
