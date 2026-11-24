/*
 * plugins_cmd_remove.c -- faithful C11 port of the deterministic dashboard
 * removal decision in hermes_cli/plugins_cmd_remove.py
 * (wave17 gate w17_pluginscmd_207eca).
 *
 * PoP: w17_pluginscmd_207eca_dashboard_remove_error @ hermes_cli/plugins_cmd_remove.py:dashboard_remove_user_plugin
 *
 * The tree mutation half needs a live filesystem and stays in Python. The
 * exported helper implements the two refusal branches exactly, including the
 * bundled-source guard and the f-string error text.
 */
#include "noc_w17_pluginscmd_207eca.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* All string values are copied into fresh json_string nodes while `entries`
 * is still alive -- never retain an interior pointer past json_free(). */

/* PoP: w17_pluginscmd_207eca_dashboard_remove_error @ hermes_cli/plugins_cmd_remove.py:dashboard_remove_user_plugin */
json_t *w17_pluginscmd_207eca_dashboard_remove_error(const char *name, json_t *entries,
                                                     const char *plugins_dir)
{
    const char *who = name ? name : "";
    const char *dir = plugins_dir ? plugins_dir : "";
    json_t *out = json_object();

    /* any(n == name and src == "bundled" for n, _ver, _d, src, _path, _key in entries) */
    if (json_is_array(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            json_t *e = json_get(entries, i);
            if (!json_is_array(e)) continue;
            json_t *en = json_get(e, 0);
            json_t *src = json_get(e, 3);
            if (json_is_string(en) && json_is_string(src)) {
                const char *en_s = json_string_value(en);
                const char *src_s = json_string_value(src);
                if (en_s && src_s && strcmp(en_s, who) == 0 &&
                    strcmp(src_s, "bundled") == 0) {
                    json_set(out, "ok", json_bool(0));
                    json_set(out, "error", json_string(
                        "Bundled plugins cannot be removed from the dashboard."));
                    return out;
                }
            }
        }
    }

    /* target is None -> f"Plugin '{name}' was not found under {plugins_dir}." */
    size_t need = strlen(who) + strlen(dir) + 64;
    char *msg = (char *)malloc(need);
    if (msg) {
        snprintf(msg, need, "Plugin '%s' was not found under %s.", who, dir);
        json_set(out, "error", json_string(msg));
        free(msg);
    } else {
        json_set(out, "error", json_string(""));
    }
    json_set(out, "ok", json_bool(0));
    return out;
}

