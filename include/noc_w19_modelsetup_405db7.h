/* noc_w19_modelsetup_405db7.h - public C API for hermes_cli/model_setup_flows_bedrock.py port.
 * Only the pure policy helpers live here; the interactive wizard flows stay Python-side. */
#ifndef NOC_W19_MODELSETUP_405DB7_H
#define NOC_W19_MODELSETUP_405DB7_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_modelsetup_405db7_region_geo_prefix @ hermes_cli/model_setup_flows_bedrock.py:bedrock_region_geo_prefix
 * AWS region name -> inference-profile geo prefix ("us.", "eu.", ...); "" when unknown. */
const char *w19_modelsetup_405db7_region_geo_prefix(const char *region_name);

/* PoP: w19_modelsetup_405db7_model_routable @ hermes_cli/model_setup_flows_bedrock.py:bedrock_model_routable_from_region
 * Bare foundation-model ids and global.* profiles route anywhere; a geo-prefixed profile
 * only from its own geography (ap. accepts ap./apac./jp.). Unknown region hides nothing. */
int w19_modelsetup_405db7_model_routable(const char *model_id, const char *region_name);

/* PoP: w19_modelsetup_405db7_text_model_ids @ hermes_cli/model_setup_flows_bedrock.py:_bedrock_text_model_ids
 * Filter live discovery records (array of objects carrying "id") to routable text models,
 * dedupe bare ids against their inference profiles, and order recommended-in-region first. */
json_t *w19_modelsetup_405db7_text_model_ids(const json_t *live_models, const char *region);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_MODELSETUP_405DB7_H */
