/*
 * plugins_cmd_listing.c -- faithful C11 port of _filter_plugin_entries in
 * hermes_cli/plugins_cmd_listing.py (wave18 gate w18_pluginscmd_1fa477).
 *
 * PoP: w18_pluginscmd_1fa477_filter_entries @ hermes_cli/plugins_cmd_listing.py:_filter_plugin_entries
 *
 * The facade lookup (_category_active_names / _plugin_status) is injected by
 * the harness as a name->status map; the filtering itself is ported verbatim.
 * All input keys are echoed by the harness before this module's entry points.
 */
#include "noc_w18_pluginscmd_1fa477.h"

#include <string.h>

json_t *w18_pluginscmd_1fa477_filter_entries(json_t *entries, bool no_bundled, bool user,
                                             bool enabled, json_t *status_map)
{
    json_t *out = json_array();
    if (!out) return NULL;
    size_t n = json_is_array(entries) ? json_len(entries) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(entries, i);
        int keep = 1;

        /* if getattr(args, "no_bundled", False) or getattr(args, "user", False):
         *     filtered = [e for e in filtered if e[3] != "bundled"] */
        if (no_bundled || user) {
            json_t *src = json_is_array(e) ? json_get(e, 3) : NULL;
            if (src && json_is_string(src) && src->str_val
                && strcmp(src->str_val, "bundled") == 0)
                keep = 0;
        }

        /* if getattr(args, "enabled", False): keep status == "enabled" */
        if (keep && enabled) {
            json_t *nm = json_is_array(e) ? json_get(e, 0) : NULL;
            const char *name = (nm && json_is_string(nm) && nm->str_val) ? nm->str_val : "";
            json_t *st = json_obj_get(status_map, name);
            const char *status = (st && json_is_string(st) && st->str_val) ? st->str_val : "-";
            if (strcmp(status, "enabled") != 0) keep = 0;
        }

        if (keep && e) json_append(out, json_copy(e));
    }
    return out;
}
