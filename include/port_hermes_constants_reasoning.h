/*
 * port_hermes_constants_reasoning.h — Faithful C11 ports of the reasoning-effort
 * resolution chokepoint from hermes_constants.py:
 *   _canonical_model_variants  → tolerant model-name spelling variants
 *   resolve_per_model_reasoning_effort → per-model override lookup
 *   resolve_reasoning_config   → the shared chokepoint (per-model > global)
 *
 * The leaf parser parse_reasoning_effort lives in port_slash_commands.c
 * (reasoning_parse_effort); this file builds the resolution layer on top of it.
 */

#ifndef PORT_HERMES_CONSTANTS_REASONING_H
#define PORT_HERMES_CONSTANTS_REASONING_H

#include "hermes_json.h"

/* _canonical_model_variants(model) — bounded spelling variants for tolerant
 * override matching (exact, dots↔dashes, version-dot recovery, bare model,
 * aggregator strip, known-prefix prepend). Returns a malloc'd NULL-terminated
 * array of malloc'd strings. Caller frees each element and the array. Returns
 * NULL on empty input. *out_count receives the element count (may be NULL). */
char **reasoning_canonical_model_variants(const char *model, int *out_count);

/* resolve_per_model_reasoning_effort(model, overrides) — look up a per-model
 * reasoning_effort override with spelling tolerance. overrides is the config
 * dict agent.reasoning_overrides. Returns a malloc'd json_t reasoning-config
 * object (see reasoning_parse_effort), or NULL when no match. */
json_t *reasoning_resolve_per_model(const char *model, const json_t *overrides);

/* resolve_reasoning_config(cfg, model) — the shared resolution chokepoint.
 * Priority: per-model override from agent.reasoning_overrides, then global
 * agent.reasoning_effort (raw value preserved so YAML False = disabled). When
 * model is empty it is derived from cfg's model section (string, or dict
 * default/model keys). Returns a malloc'd json_t reasoning-config object, or
 * NULL when unset/unrecognized (caller uses the provider default). */
json_t *reasoning_resolve_config(const json_t *cfg, const char *model);

#endif /* PORT_HERMES_CONSTANTS_REASONING_H */
