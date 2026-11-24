#ifndef NOC_W20_MODELS_59CC22_H
#define NOC_W20_MODELS_59CC22_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/teams_pipeline/models.py (gate w20_models_59cc22).
 *
 * The module is pure stdlib (dataclasses + datetime), so the whole normalized
 * model layer is ported: the conversion helpers, the snake/camel aliasing
 * `from_dict`, the None-dropping `to_dict`, and every dataclass's required /
 * datetime / artifact_type validation.
 *
 * A model instance travels as a JSON object of field -> value, with datetime
 * fields already in the UTC `...Z` wire form that `to_dict` emits.
 */

/* PoP: w20_models_59cc22_camel @ plugins/teams_pipeline/models.py:_camel
 * `head + "".join(part.title() for part in rest)` over name.split("_"). */
char *w20_models_59cc22_camel(const char *name);

/* PoP: w20_models_59cc22_str @ plugins/teams_pipeline/models.py:_str
 * `str(value or "").strip()`; NULL reads as None. Caller frees. */
char *w20_models_59cc22_str(const json_t *value);

/* PoP: w20_models_59cc22_pick @ plugins/teams_pipeline/models.py:_pick
 * `payload.get(k1) or payload.get(k2) or ...`, falling back to the LAST key's
 * value when every candidate is falsy. Returns a fresh node (never NULL). */
json_t *w20_models_59cc22_pick(const json_t *payload, const char *const *keys, size_t nkeys);

/* PoP: w20_models_59cc22_parse_datetime @ plugins/teams_pipeline/models.py:_parse_datetime
 * Parse ISO-8601 (Z accepted; naive assumed UTC) then serialize back in UTC:
 *   {"status":"ok","value":"2024-01-01T08:00:00Z"} or
 *   {"status":"error","type":"ValueError","message":...}. */
json_t *w20_models_59cc22_parse_datetime(const char *value);

/* PoP: w20_models_59cc22_from_dict @ plugins/teams_pipeline/models.py:_Model.from_dict
 * model is one of GraphSubscription, TeamsMeetingRef, MeetingArtifact,
 * TeamsMeetingSummaryPayload, TeamsMeetingPipelineJob:
 *   {"status":"ok","value":{...}} | {"status":"error","type":...,"message":...}. */
json_t *w20_models_59cc22_from_dict(const char *model, const json_t *payload);

/* PoP: w20_models_59cc22_to_dict @ plugins/teams_pipeline/models.py:_Model.to_dict
 * The None-dropping serializer: datetimes -> ISO Z, nested models -> dicts,
 * empty lists/dicts dropped. Unknown model -> null. */
json_t *w20_models_59cc22_to_dict(const char *model, const json_t *obj);

/* PoP: w20_models_59cc22_roundtrip @ plugins/teams_pipeline/models.py:_Model.to_dict
 * `Model.from_dict(payload).to_dict()` in one step (what the wire layer does). */
json_t *w20_models_59cc22_roundtrip(const char *model, const json_t *payload);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_MODELS_59CC22_H */
