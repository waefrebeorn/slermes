/*
 * noc_w25_conversati_616874.h - C surface for upstream
 * agent/conversation_compression_deadline.py (gate w25_conversati_616874).
 *
 * The configured compression limits and the fence deadline arithmetic.  The
 * config file, the auxiliary client and the commit fence are the seam: the
 * compression config dict and the effective aux budget arrive as values, and
 * what is ported is what those values decide.
 */
#ifndef NOC_W25_CONVERSATI_616874_H
#define NOC_W25_CONVERSATI_616874_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (cfg, aux_budget) -> [idle_timeout_seconds, total_ceiling_seconds].
 * cfg null means "no compression config on disk". */
json_t *w25_conversati_616874_resolve_timeouts(const json_t *cfg, const json_t *aux_budget);

/* (fence_deadline, ceiling, wait_started) -> the total limit for this wait. */
json_t *w25_conversati_616874_effective_ceiling(const json_t *fence_deadline,
                                                const json_t *ceiling,
                                                const json_t *wait_started);

/* The default limits as an object. */
json_t *w25_conversati_616874_constants(void);

#ifdef __cplusplus
}
#endif
#endif
