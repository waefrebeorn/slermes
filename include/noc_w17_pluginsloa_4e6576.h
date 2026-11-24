#ifndef NOC_W17_PLUGINSLOA_4E6576_H
#define NOC_W17_PLUGINSLOA_4E6576_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_pluginsloa_4e6576_guardable_names @ hermes_cli/plugins_load_guard.py:ignore_registrars_after_abandoned_load
 * Input: array of arrays of attribute names, one inner array per class in the MRO.
 * Output: the sorted, deduplicated names that the decorator wraps:
 *   name.startswith("register_") or name in {"subscribe", "on_unload"}.
 */
struct json_t *w17_pluginsloa_4e6576_guardable_names(const struct json_t *sets);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSLOA_4E6576_H */
