/*
 * plugins_ledger.c — C home for upstream Hermes hermes_cli/plugins_ledger.py.
 * PoP: w12_pluginsled_05da3f_remove_identity  @ hermes_cli/plugins_ledger.py:PluginLedgerMixin._remove_identity
 * PoP: w12_pluginsled_05da3f_remove_callback  @ hermes_cli/plugins_ledger.py:PluginLedgerMixin._remove_callback
 * PoP: w12_pluginsled_05da3f_restore_mapping  @ hermes_cli/plugins_ledger.py:PluginLedgerMixin._restore_mapping
 *
 * Registrations/callbacks are represented by unique opaque token strings; a
 * target token is the very object at target_index, so "last exact object match"
 * equals "last equal token" (fixtures never reuse a token non-last).
 */
#include "noc_w12_pluginsled_05da3f.h"
#include <stdio.h>
#include <string.h>

static bool remove_last_identity(const json_t *items, size_t target_index,
                                 json_t *out_arr)
{
    size_t n, last = (size_t)-1, i;
    const json_t *tj;
    const char *target = NULL;
    bool removed = false;
    if (!items || items->type != JSON_ARRAY) return false;
    n = json_len(items);
    tj = (target_index < n) ? json_get(items, target_index) : NULL;
    if (tj && tj->type == JSON_STRING) target = tj->str_val;
    if (target) {
        for (i = 0; i < n; i++) {
            const json_t *v = json_get(items, i);
            if (v && v->type == JSON_STRING && v->str_val &&
                !strcmp(v->str_val, target)) last = i;
        }
    }
    for (i = 0; i < n; i++) {
        if (i == last) { removed = true; continue; }
        json_append(out_arr, json_copy(json_get(items, i)));
    }
    return removed;
}

json_t *w12_pluginsled_05da3f_remove_identity(const json_t *items, size_t target_index)
{
    json_t *out = json_object();
    json_t *arr = json_array();
    bool removed = remove_last_identity(items, target_index, arr);
    json_set(out, "items", arr);
    json_set(out, "removed", json_bool(removed));
    return out;
}

json_t *w12_pluginsled_05da3f_remove_callback(const json_t *items, size_t target_index)
{
    json_t *out = json_object();
    json_t *arr = json_array();
    remove_last_identity(items, target_index, arr);
    json_set(out, "items", arr);
    json_set(out, "has_key", json_bool(json_len(arr) > 0));
    return out;
}

json_t *w12_pluginsled_05da3f_restore_mapping(const char *map_value, const char *current,
                            const char *previous)
{
    json_t *out = json_object();
    int same = (map_value == NULL && current == NULL) ||
               (map_value && current && !strcmp(map_value, current));
    const char *value = map_value;
    if (!same) {
        json_set(out, "value", map_value ? json_string(map_value) : json_null());
        json_set(out, "changed", json_bool(false));
        return out;
    }
    value = previous;               /* NULL previous pops the key */
    json_set(out, "value", value ? json_string(value) : json_null());
    json_set(out, "changed", json_bool(true));
    return out;
}
