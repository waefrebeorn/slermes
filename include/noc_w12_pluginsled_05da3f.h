/* noc_w12_pluginsled_05da3f.h — public C API port of hermes_cli/plugins_ledger.py */
#ifndef NOC_W12_PLUGINSLED_05DA3F_H
#define NOC_W12_PLUGINSLED_05DA3F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PluginLedgerMixin._remove_identity: drop the last element equal to the
 * element at target_index. Emits {"items": [...], "removed": bool}. */
json_t *w12_pluginsled_05da3f_remove_identity(const json_t *items, size_t target_index);

/* PluginLedgerMixin._remove_callback: same removal on a single mapping entry;
 * emits {"items": [...], "has_key": bool} (key popped when it goes empty). */
json_t *w12_pluginsled_05da3f_remove_callback(const json_t *items, size_t target_index);

/* PluginLedgerMixin._restore_mapping: identity-conditional restore of one
 * manager-local mapping slot. Emits {"value": str|null, "changed": bool}. */
json_t *w12_pluginsled_05da3f_restore_mapping(const char *map_value, const char *current,
                            const char *previous);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_PLUGINSLED_05DA3F_H */
