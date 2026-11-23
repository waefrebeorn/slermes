/*
 * isolated_scope.c — C home for upstream Hermes hermes_cli/isolated_scope.py.
 *
 * PoP: w12_isolatedsc_454cd5_filter  @ hermes_cli/isolated_scope.py:scope_to_launch_profile
 * PoP: w12_isolatedsc_454cd5_record  @ hermes_cli/isolated_scope.py:record_launch_scope
 *
 * UX-only listing scope. The hermes_constants lookups are injected seams
 * (the live function late-imports them and resolves both homes).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_isolatedsc_454cd5.h"

json_t *w12_isolatedsc_454cd5_filter(const json_t *scope, const json_t *items,
                                     const char *key) {
    json_t *out = json_array();
    if (!items || items->type != JSON_ARRAY) return out;
    int unscoped = (!scope || scope->type == JSON_NULL);
    const char *want = (scope && scope->type == JSON_STRING) ? scope->str_val : NULL;
    for (size_t i = 0; i < json_len(items); ++i) {
        const json_t *item = json_get(items, i);
        if (unscoped) { json_append(out, json_copy(item)); continue; }
        const char *nm = NULL;
        if (key && *key && json_is_object(item)) {
            const json_t *v = json_obj_get(item, key);
            if (v && v->type == JSON_STRING) nm = v->str_val;
        } else if (item && item->type == JSON_STRING) {
            nm = item->str_val;
        }
        if (nm && want && !strcmp(nm, want)) json_append(out, json_copy(item));
    }
    return out;
}

json_t *w12_isolatedsc_454cd5_record(int isolated, int desktop_owned,
                                     const char *home, const char *default_root,
                                     const char *profile) {
    if (!isolated || desktop_owned) return json_null();
    if (!strcmp(home ? home : "", default_root ? default_root : "")) return json_null();
    return json_string(profile ? profile : "");
}
