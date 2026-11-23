/*
 * plugin_utils.c — C port of upstream Hermes plugins/plugin_utils.py.
 *
 * PoP: w7_pluginutils_c0e546_all @ plugins/plugin_utils.py:__all__
 * PoP: w7_pluginutils_c0e546_singleton_slot_ops @ plugins/plugin_utils.py:SingletonSlot.get/peek/reset
 *
 * SingletonSlot is a thread-safe lazy slot ("first config wins"). The C port
 * models its deterministic single-thread contract: a separate `set` flag from
 * the cached value (so a factory returning None still counts as cached) and a
 * factory call counter that persists across reset().
 */
#include "noc_w7_pluginutils_c0e546.h"
#include "hermes_json.h"
#include <string.h>

struct json_t *w7_pluginutils_c0e546_all(void) {
    json_t *arr = json_array();
    json_append(arr, json_string("SingletonSlot"));
    json_append(arr, json_string("lazy_singleton"));
    return arr;
}

struct json_t *w7_pluginutils_c0e546_singleton_slot_ops(const json_t *ops,
                                                        const json_t *factory_values) {
    json_t *out = json_array();
    json_t *cached = NULL;
    int set = 0;
    size_t calls = 0;
    size_t nv = json_is_array(factory_values) ? json_len(factory_values) : 0;
    size_t nops = json_is_array(ops) ? json_len(ops) : 0;
    for (size_t i = 0; i < nops; i++) {
        const json_t *en = json_get(ops, i);
        const char *op = json_is_string(en) ? en->str_val : "";
        if (!strcmp(op, "get")) {
            if (!set) {
                cached = calls < nv ? json_copy(json_get(factory_values, calls)) : json_null();
                calls++;
                set = 1;
            }
            json_append(out, json_copy(cached));
        } else if (!strcmp(op, "peek")) {
            json_append(out, set ? json_copy(cached) : json_null());
        } else if (!strcmp(op, "reset")) {
            json_free(cached);
            cached = NULL;
            set = 0;
            json_append(out, json_null());
        } else {
            json_append(out, json_null());
        }
    }
    json_free(cached);
    return out;
}
