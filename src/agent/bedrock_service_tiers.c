/*
 * bedrock_service_tiers.c — C port of agent/bedrock_service_tiers.py.
 * PoP: bedtier_bedrock_fast_mode_overrides @ agent/bedrock_service_tiers.py:bedrock_fast_mode_overrides
 * PoP: BEDROCK_ULTRAFAST_ROUTES @ agent/bedrock_service_tiers.py:BEDROCK_ULTRAFAST_ROUTES
 *
 * Python:
 *   routes = BEDROCK_ULTRAFAST_ROUTES.get(model_id or "", ())
 *   if tier == "ultrafast" and str(base_url or "").rstrip("/") in routes:
 *       return {"service_tier": "ultrafast"}
 *   return None
 */
#include "noc_bedtier.h"
#include <string.h>

#define BEDTIER_ULTRA_MODEL "openai.gpt-6.1-sol"
#define BEDTIER_ULTRA_URL   "https://bedrock-mantle.us-east-1.api.aws/openai/v1"

const char *bedtier_bedrock_fast_mode_overrides(const char *model_id,
                                                const char *base_url,
                                                const char *tier) {
    const char *model = model_id ? model_id : "";
    if (strcmp(model, BEDTIER_ULTRA_MODEL) != 0) return NULL;
    if (!tier || strcmp(tier, "ultrafast") != 0) return NULL;

    const char *url = base_url ? base_url : "";
    static char buf[4096];
    size_t n = strlen(url);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, url, n);
    while (n > 0 && buf[n - 1] == '/') n--; /* str.rstrip("/") */
    buf[n] = '\0';
    if (strcmp(buf, BEDTIER_ULTRA_URL) == 0) return "ultrafast";
    return NULL;
}
