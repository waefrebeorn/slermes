/*
 * noc_w27_adapter_e9cb6b_p6.h - C surface for the p6 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p6).
 *
 * The adapter's numeric budgets and dispatch tables (reaction emoji, approval
 * map, YAML bridge rows, media extension sets), the standalone txn-id builder
 * and the bounded event-dedup ring.  Reading them from config/env, and the
 * HTTP PUT that consumes a txn id, are seams.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P6_H
#define NOC_W27_ADAPTER_E9CB6B_P6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {max_content_bytes, max_message_length_ceiling, default_max_message_length,
 *  split_threshold, startup_grace_seconds, voice_waveform_bins,
 *  exec_approval_cmd_budget} */
json_t *w27_adapter_constants(void);

/* _MATRIX_MODEL_PICKER_REACTIONS as an array. */
json_t *w27_adapter_model_picker_reactions(void);

/* _MATRIX_CHOICE_PICKER_REACTIONS as an array. */
json_t *w27_adapter_choice_picker_reactions(void);

/* _approval_reaction_map as an object. */
json_t *w27_adapter_approval_reaction_map(void);

/* _YAML_BRIDGE as [[yaml_key, env_var, kind], ...]. */
json_t *w27_adapter_yaml_bridge(void);

/* _MATRIX_IMAGE_FILENAME_EXTS, sorted. */
json_t *w27_adapter_image_exts(void);

/* _MATRIX_MEDIA_FILENAME_EXTS, sorted. */
json_t *w27_adapter_media_exts(void);

/* (epoch_ms, 8 hex chars) -> "hermes_<ms>_<hex>". */
json_t *w27_adapter_txn_id(const json_t *ms, const json_t *rand_hex);

/* [event_id, ...] -> [was_duplicate, ...] over a 1000-entry ring. */
json_t *w27_adapter_duplicate_events(const json_t *ids);

#ifdef __cplusplus
}
#endif
#endif
