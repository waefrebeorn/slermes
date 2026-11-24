/*
 * noc_w27_adapter_1f12c3_p2.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p2).
 *
 * config/liveness knob resolvers and preview shaping (adapter.py:440-1400).
 * PORTED (pure): the non-conversational id set membership test, the DAVE
 * downgrade flag, the extra->env->default knob resolver (_config_value,
 * _liveness_knob via _finite_positive_config_float / _config_int) and the
 * clickable tool-preview shaping.
 * SEAM: os.getenv (the env value is the parameter) and the adapter config object.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P2_H
#define NOC_W27_ADAPTER_1F12C3_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p2_tracker_contains(const json_t *ids, const json_t *message_id);
json_t *w27_adapter_1f12c3_p2_voice_receiver_dave_downgraded(const json_t *creds);
json_t *w27_adapter_1f12c3_p2_config_value(const json_t *extra, const json_t *key, const json_t *dflt, const json_t *env);
json_t *w27_adapter_1f12c3_p2_finite_positive_config_float(const json_t *extra, const json_t *key, const json_t *dflt, const json_t *env);
json_t *w27_adapter_1f12c3_p2_config_int(const json_t *extra, const json_t *key, const json_t *dflt, const json_t *env);
json_t *w27_adapter_1f12c3_p2_format_tool_preview(const json_t *preview);

#ifdef __cplusplus
}
#endif
#endif

