#ifndef NOC_W6_MCPLIVENESS_08BEC8_H
#define NOC_W6_MCPLIVENESS_08BEC8_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: mcpliveness_action @ tools/mcp_liveness.py:_action
 * Returns [user_action, retry] for an unavailable state. Caller frees. */
json_t *mcpliveness_action(const char *state, const char *app_name, const char *gpu_label);

/* PoP: mcpliveness_availability_problem @ tools/mcp_liveness.py:availability_problem
 * Returns the LivenessState an availability.state maps to, else JSON null. */
json_t *mcpliveness_availability_problem(const char *state);

/* PoP: mcpliveness_parse @ tools/mcp_liveness.py:parse_liveness
 * `facts` supplies "raw" (the declaration) and "osf" (the OS family injected
 * for location_is_rooted). Returns {"ok":true,...liveness fields...} or
 * {"ok":false,"error":<ValueError text>}. Caller frees. */
json_t *mcpliveness_parse(const json_t *facts);

/* PoP: mcpliveness_describe @ tools/mcp_liveness.py:describe
 * `facts` supplies name/required_gpu/available{state,min_version,version}/
 * liveness_state/display_name. Returns the composed sentence. Caller frees. */
json_t *mcpliveness_describe(const json_t *facts);

#ifdef __cplusplus
}
#endif
#endif
