#ifndef NOC_W10_CLITUIMIX_9A2C8A_H
#define NOC_W10_CLITUIMIX_9A2C8A_H

/* PoP: w10_clituimix_9a2c8a_num_prefix @ hermes_cli/cli_tui_mixin.py:_num_prefix
 * PoP: w10_clituimix_9a2c8a_approval_choice_label @ hermes_cli/cli_tui_mixin.py:_approval_choice_label
 * PoP: w10_clituimix_9a2c8a_tn @ hermes_cli/cli_tui_mixin.py:_tn
 * PoP: w10_clituimix_9a2c8a_audio_bar @ hermes_cli/cli_tui_mixin.py:CLITuiMixin._audio_level_bar */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Quick-select key label: str(i+1) for i<9, "0" for i==9, blank beyond. */
struct json_t *w10_clituimix_9a2c8a_num_prefix(const struct json_t *i);

/* Approval-choice label: catalog key for a known protocol token, else verbatim.
 * The i18n lookup t() is the identity on its key (injected seam). */
struct json_t *w10_clituimix_9a2c8a_choice_label(const struct json_t *choice);

/* Plural-aware lookup: <key>_one for count==1, <key>_other otherwise. */
struct json_t *w10_clituimix_9a2c8a_tn(const struct json_t *key, const struct json_t *count);

/* Audio level indicator from RMS: " ▁▂▃▄▅▆▇"[min(rms,8000)*7//8000]. */
struct json_t *w10_clituimix_9a2c8a_audio_bar(const struct json_t *rms);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_CLITUIMIX_9A2C8A_H */
