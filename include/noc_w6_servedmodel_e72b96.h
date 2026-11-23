#ifndef NOC_W6_SERVEDMODEL_E72B96_H
#define NOC_W6_SERVEDMODEL_E72B96_H

/*
 * Public C API for the port of agent/served_model.py (gate w6_servedmodel_e72b96).
 * All exported symbols are servedmodel_-prefixed.
 */
#include "hermes_json.h"

/* Port of served_model_from_headers(headers): first non-empty value among
 * SERVED_MODEL_HEADERS, coerced with str() and .strip(), or JSON null.
 * `headers` is a JSON object (or NULL for None). Caller frees the result. */
json_t *servedmodel_from_headers(const json_t *headers);

/* Port of result_model_fields(agent): the requested/served model pair.
 * last_served: JSON string|null (agent.last_served_model).
 * model: agent.model (string).
 * fallback_activated: int bool (agent._fallback_activated).
 * primary_runtime: JSON object|null (agent._primary_runtime).
 * Returns {"requested_model":str|null,"served_model":str|null}. Caller frees. */
json_t *servedmodel_result_fields(const json_t *last_served, const char *model,
                                  int fallback_activated, const json_t *primary_runtime);

#endif /* NOC_W6_SERVEDMODEL_E72B96_H */
