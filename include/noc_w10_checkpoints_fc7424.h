#ifndef NOC_W10_CHECKPOINTS_FC7424_H
#define NOC_W10_CHECKPOINTS_FC7424_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_checkpoints_fc7424_load_enabled @ tui_gateway/checkpoints.py:_load_checkpoints_enabled
 * 1/0 for bool. cfg is the config object (or NULL). */
int w10_checkpoints_fc7424_load_enabled(const struct json_t *cfg);
/* PoP: w10_checkpoints_fc7424_resolve_enabled_ex @ tui_gateway/checkpoints.py:resolve_checkpoints_enabled
 * Explicit seam: env_value is the value of HERMES_TUI_CHECKPOINTS (NULL = unset).
 * 1/0 for bool. */
int w10_checkpoints_fc7424_resolve_enabled_ex(const struct json_t *cfg, const struct json_t *env_value);
/* PoP: w10_checkpoints_fc7424_undo_hint_fields @ tui_gateway/checkpoints.py:undo_hint_fields
 * hint_seam is the value undo_rollback_hint_for would return for an enabled manager.
 * Returns a JSON object; caller frees. */
struct json_t *w10_checkpoints_fc7424_undo_hint_fields(const struct json_t *session,
        const struct json_t *rewound, const struct json_t *params, const struct json_t *hint_seam);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_CHECKPOINTS_FC7424_H */
