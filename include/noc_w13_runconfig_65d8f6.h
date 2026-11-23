#ifndef NOC_W13_RUNCONFIG_65D8F6_H
#define NOC_W13_RUNCONFIG_65D8F6_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_runconfig_65d8f6_busy_modes_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._busy_modes_from_config
 * Returns [input_mode, text_mode] from a config dict and two fallback strings. */
struct json_t *w13_runconfig_65d8f6_busy_modes_from_config(const struct json_t *config,
                                                           const struct json_t *fallback_input,
                                                           const struct json_t *fallback_text);
/* PoP: w13_runconfig_65d8f6_busy_text_timing_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._busy_text_timing_from_config
 * Returns [debounce_seconds, hard_cap_seconds]; invalid/negative values use the defaults. */
struct json_t *w13_runconfig_65d8f6_busy_text_timing_from_config(const struct json_t *config);
/* PoP: w13_runconfig_65d8f6_human_delay_from_config @ gateway/run_config_loaders.py:GatewayConfigLoadersMixin._human_delay_from_config
 * Returns [lo_ms, hi_ms] or null (mode off). */
struct json_t *w13_runconfig_65d8f6_human_delay_from_config(const struct json_t *config);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_RUNCONFIG_65D8F6_H */
