#ifndef NOC_W10_DEBUGSUMMA_BC82F8_H
#define NOC_W10_DEBUGSUMMA_BC82F8_H

/* PoP: w10_debugsumma_bc82f8_config_shape @ hermes_cli/debug_summary.py:_config_shape
 * PoP: w10_debugsumma_bc82f8_toolset_names @ hermes_cli/debug_summary.py:_toolset_names */
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Key-name-only shape of raw config.yaml (never values), restricted to keys
 * present in schema, recursing at most depth 2 (MAX_SHAPE_DEPTH). */
struct json_t *w10_debugsumma_bc82f8_config_shape(const struct json_t *raw,
                                                  const struct json_t *schema);

/* Configured toolset names: built-ins named (sorted), user-named custom ones
 * counted. `toolsets` is the live TOOLSETS set injected as a JSON string array. */
struct json_t *w10_debugsumma_bc82f8_toolset_names(const struct json_t *raw_config,
                                                   const struct json_t *toolsets);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_DEBUGSUMMA_BC82F8_H */
