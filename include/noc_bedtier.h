#ifndef NOC_BEDTIER_H
#define NOC_BEDTIER_H

/* C port of agent/bedrock_service_tiers.py.
 * PoP: bedtier_bedrock_fast_mode_overrides @ agent/bedrock_service_tiers.py:bedrock_fast_mode_overrides
 * PoP: BEDROCK_ULTRAFAST_ROUTES @ agent/bedrock_service_tiers.py:BEDROCK_ULTRAFAST_ROUTES
 *
 * Returns "ultrafast" when the documented model/endpoint/tier triple opts in
 * (Python {"service_tier": "ultrafast"}), else NULL (Python None).
 */
const char *bedtier_bedrock_fast_mode_overrides(const char *model_id,
                                                const char *base_url,
                                                const char *tier);

#endif /* NOC_BEDTIER_H */
