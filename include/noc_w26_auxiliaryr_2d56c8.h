/*
 * noc_w26_auxiliaryr_2d56c8.h - C surface for upstream
 * agent/auxiliary_reasoning_floor.py (gate w26_auxiliaryr_2d56c8).
 *
 * The reasoning floor for auxiliary (fast, thinking-off) requests: the route
 * key a retry is memoised under, the "is this shape a disable?" test, the
 * lift-to-floor rewrite of every thinking-off encoding, and the budgets the
 * ladder is tuned with.  The process-wide memo of floored routes and the
 * /v1/models catalog are seams: the caller passes their answers as values.
 */
#ifndef NOC_W26_AUXILIARYR_2D56C8_H
#define NOC_W26_AUXILIARYR_2D56C8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* REASONING_FLOOR_EFFORT: the lowest effort every reasoning wire accepts. */
#define W26_AUXILIARYR_2D56C8_FLOOR_EFFORT "low"

/* _route_key(provider, base_url) -> str: endpoint host:port when a base_url
 * override names one, else the normalised provider name. */
json_t *w26_auxiliaryr_2d56c8_route_key(const json_t *provider, const json_t *base_url);

/* _is_disabled(reasoning_config) -> bool: a dict whose "enabled" is literally
 * False (0 is not False). */
json_t *w26_auxiliaryr_2d56c8_is_disabled(const json_t *reasoning_config);

/* floor_reasoning_config(reasoning_config) -> value: the disabled config lifted
 * to {"enabled": true, "effort": "low"}; anything else returned as-is. */
json_t *w26_auxiliaryr_2d56c8_floor_config(const json_t *reasoning_config);

/* with_reasoning_floor(kwargs) -> object|null: a copy of kwargs with every
 * thinking-off encoding lifted to the floor, or null when nothing was
 * disabled (the ladder never re-sends an unchanged request). */
json_t *w26_auxiliaryr_2d56c8_with_floor(const json_t *kwargs);

/* known_reasoning_floor(...): the config lifted when this route+model is known
 * to refuse a disable.  `known` is the process memo's answer (the route+model
 * pair was floored before), `catalog` the /v1/models `reasoning.mandatory`
 * flag; both are seams supplied as values. */
json_t *w26_auxiliaryr_2d56c8_known_floor(const json_t *reasoning_config, const json_t *provider,
                                          const json_t *base_url, const json_t *model,
                                          int known, int catalog);

/* The floor effort and the disabled-effort vocabulary, as an object. */
json_t *w26_auxiliaryr_2d56c8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
