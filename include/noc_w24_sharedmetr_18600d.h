/*
 * noc_w24_sharedmetr_18600d.h - C surface for upstream
 * hermes_cli/web_routers/shared_metrics.py (gate w24_sharedmetr_18600d).
 *
 * The dashboard's first-run shared-metrics offer, ported as the values it
 * decides: the request model's field validation (``ConsentAnswer``), the
 * ``{enabled, send, decided, reask, managed}`` view the GET answers with, the
 * mapping mutation the PUT performs on a config document, and the offer
 * contract (choices, offer version, docs URL) every surface shows.
 * The HTTP router, the thread hop and the config file are seams: this file
 * only ever sees values.
 */
#ifndef W24_SHAREDMETR_18600D_H
#define W24_SHAREDMETR_18600D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Offer version that settles an answer (bumped after the type-ahead fix). */
#define W24_SHAREDMETR_18600D_OFFER_VERSION 2
/* Index of "No thanks" in the offer's choice table (the safe default). */
#define W24_SHAREDMETR_18600D_NO_THANKS 1

/* {"enabled": <value>, "send": <value>?} -> ConsentAnswer.model_dump()
 * = {"enabled": bool, "send": bool}; a value the model refuses answers
 * {"error": "ValidationError"}. */
json_t *w24_sharedmetr_18600d_answer(const json_t *req);

/* {"config": <raw config mapping>, "managed": <bool>} ->
 * {**consent_state(config), "managed": bool}. */
json_t *w24_sharedmetr_18600d_consent_view(const json_t *req);

/* {"target": <mapping>, "enabled": <bool>, "send": <bool>} -> the mapping after
 * set_answer(): telemetry.shared_metrics gets enabled, send (send & enabled)
 * and offer_version. */
json_t *w24_sharedmetr_18600d_set_answer(const json_t *req);

/* {"offer_version", "choices", "no_thanks", "docs_url"} as an object. */
json_t *w24_sharedmetr_18600d_constants(void);

#ifdef __cplusplus
}
#endif
#endif
