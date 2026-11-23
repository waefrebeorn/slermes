#ifndef NOC_W8_CONFIGLOADE_847BBE_H
#define NOC_W8_CONFIGLOADE_847BBE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_configloade_847bbe_bridge_lookup @ gateway/config_loader.py:_bridge_lookup
 * Returns a 2-element JSON array [bool found, value|null]. Caller frees. */
struct json_t *w8_configloade_847bbe_bridge_lookup(const struct json_t *yaml_cfg,
    const struct json_t *gateway_section, const struct json_t *gw_data,
    const char *key, const char *mode);

/* PoP: w8_configloade_847bbe_quick_commands_ok @ gateway/config_loader.py:_quick_commands_ok
 * Returns JSON true for a mapping value, false otherwise (warning elided). Caller frees. */
struct json_t *w8_configloade_847bbe_quick_commands_ok(const struct json_t *value);

/* PoP: w8_configloade_847bbe_str_keyed @ gateway/config_loader.py:_str_keyed
 * JSON object keys are already strings, so this is a deep copy (non-dicts pass through). Caller frees. */
struct json_t *w8_configloade_847bbe_str_keyed(const struct json_t *value);

/* PoP: w8_configloade_847bbe_platform_section @ gateway/config_loader.py:platform_section
 * Returns [section|null, bool is_toplevel]. Caller frees. */
struct json_t *w8_configloade_847bbe_platform_section(const struct json_t *yaml_cfg,
    const char *name, const struct json_t *gateway_platforms);

/* PoP: w8_configloade_847bbe_snapshot_authored_extra @ gateway/config_loader.py:snapshot_authored_extra
 * Returns {name: extra} for every dict platform block. Caller frees. */
struct json_t *w8_configloade_847bbe_snapshot_authored_extra(const struct json_t *platforms_data);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_CONFIGLOADE_847BBE_H */
