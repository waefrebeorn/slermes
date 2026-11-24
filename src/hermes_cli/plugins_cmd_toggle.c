/*
 * plugins_cmd_toggle.c -- faithful C11 port of the pure selection logic in
 * hermes_cli/plugins_cmd_toggle.py (wave17 gate w17_pluginscmd_645b9b).
 *
 * PoP: w17_pluginscmd_645b9b_effective_plugin_selection @ hermes_cli/plugins_cmd_toggle.py:_effective_plugin_selection
 *
 * The status rule is plugins_cmd._plugin_status with dir_path == None (the
 * fixture's dir_path slot is null, so the filesystem default-on probe cannot
 * fire). Bundled platforms/backends and category-active providers are on
 * without a plugins.enabled entry; an explicit disable always wins.
 */
#include "noc_w17_pluginscmd_645b9b.h"

#include <stdlib.h>
#include <string.h>

static int array_has(json_t *arr, const char *s)
{
    if (!json_is_array(arr) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *x = json_get(arr, i);
        if (json_is_string(x)) {
            const char *xs = json_string_value(x);
            if (xs && strcmp(xs, s) == 0) return 1;
        }
    }
    return 0;
}

/* PoP: w17_pluginscmd_645b9b_effective_plugin_selection @ hermes_cli/plugins_cmd_toggle.py:_effective_plugin_selection */
json_t *w17_pluginscmd_645b9b_effective_plugin_selection(json_t *entries, json_t *enabled,
                                                         json_t *disabled, json_t *active)
{
    json_t *out = json_array();
    if (!json_is_array(entries)) return out;
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        if (!json_is_array(e)) continue;
        json_t *nv = json_get(e, 0);   /* name */
        json_t *kv = json_get(e, 5);   /* key  */
        const char *name = json_is_string(nv) ? json_string_value(nv) : NULL;
        const char *key  = json_is_string(kv) ? json_string_value(kv) : NULL;

        /* names = {name, key}; if names & disabled: "disabled" */
        if (array_has(disabled, name) || array_has(disabled, key)) continue;
        /* if names & enabled or names & active: "enabled" */
        if (array_has(enabled, name) || array_has(enabled, key) ||
            array_has(active, name) || array_has(active, key)) {
            json_append(out, json_number((double)i));
        }
        /* dir_path is null in fixtures -> _default_on never consulted. */
    }
    return out;
}

