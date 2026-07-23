#ifndef HERMES_MOA_CONFIG_H
#define HERMES_MOA_CONFIG_H

/*
 * moa_config.h — C11 port of hermes_cli/moa_config.py.
 */

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char *MOA_MARKER_PREFIX;
extern const char *MOA_DEFAULT_PRESET_NAME;

double moa_coerce_float(const json_t *value, double default_val);
long moa_coerce_int(const json_t *value, long default_val);
json_t *moa_clean_slot(const json_t *slot);
json_t *moa_default_preset(void);
json_t *moa_normalize_preset(const json_t *raw);
json_t *moa_normalize_config(const json_t *raw);
json_t *moa_list_presets(const json_t *config);
json_t *moa_resolve_preset(const json_t *config, const char *name);
const char *moa_exact_preset_name(const json_t *config, const char *text, char *out, size_t out_cap);
json_t *moa_set_active_preset(const json_t *config, const char *name);
char *moa_encode_turn(const char *prompt, const json_t *config, const char *preset);
void moa_decode_turn(const char *message, char *prompt_out, size_t prompt_cap, json_t **config_out);
char *moa_build_turn_prompt(const char *user_prompt, const json_t *config, const char *preset);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MOA_CONFIG_H */
