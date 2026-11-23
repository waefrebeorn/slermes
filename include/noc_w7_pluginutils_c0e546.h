#ifndef NOC_W7_PLUGINUTILS_C0E546_H
#define NOC_W7_PLUGINUTILS_C0E546_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_pluginutils_c0e546_all @ plugins/plugin_utils.py:__all__ */
struct json_t *w7_pluginutils_c0e546_all(void);

/* PoP: w7_pluginutils_c0e546_singleton_slot_ops @ plugins/plugin_utils.py:SingletonSlot.get/peek/reset
 * ops is a JSON array of "get"/"peek"/"reset" strings; factory_values supplies
 * the deterministic values returned by successive factory calls. Returns the
 * JSON array of results (null for reset and for an unset peek). */
struct json_t *w7_pluginutils_c0e546_singleton_slot_ops(const struct json_t *ops,
                                                        const struct json_t *factory_values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_PLUGINUTILS_C0E546_H */
