/*
 * config_provider_env.c — C home for upstream hermes_cli/config_provider_env.py.
 *
 * PoP: w18_configprov_6f4b50_entries_for @ hermes_cli/config_provider_env.py:_entries_for
 *
 * Self-contained, libjson-only.  The config/discovery-free provider symbol
 * table builder is ported; the module-level caches/hooks that call it are not
 * part of this pure function.
 */
#include "noc_w18_configprov_6f4b50.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *getstr(const struct json_t *o, const char *k) {
    const struct json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int endswith(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}
static int in_array(const struct json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); ++i) {
        const struct json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

struct json_t *w18_configprov_6f4b50_entries_for(const struct json_t *profiles,
                                                 const struct json_t *skip) {
    struct json_t *entries = json_object();
    if (!profiles || profiles->type != JSON_ARRAY) return entries;

    for (size_t i = 0; i < json_len(profiles); ++i) {
        const struct json_t *pp = json_get(profiles, i);
        if (!pp || pp->type != JSON_OBJECT) continue;
        if (strcmp(getstr(pp, "auth_type"), "api_key") != 0) continue;

        const char *label = getstr(pp, "display_name");
        if (!*label) label = getstr(pp, "name");

        const struct json_t *env_vars = json_obj_get(pp, "env_vars");
        if (!env_vars || env_vars->type != JSON_ARRAY) continue;

        for (size_t j = 0; j < json_len(env_vars); ++j) {
            const struct json_t *vn = json_get(env_vars, j);
            if (!vn || vn->type != JSON_STRING) continue;
            const char *var = vn->str_val;
            if (in_array(skip, var)) continue;
            if (json_obj_get(entries, var)) continue;      /* first declaration wins */

            int is_key = !(endswith(var, "_BASE_URL") || endswith(var, "_URL"));

            char desc[768], prm[900];
            snprintf(desc, sizeof(desc), "%s %s", label,
                     is_key ? "API key" : "base URL override");
            snprintf(prm, sizeof(prm), "%s %s", label,
                     is_key ? "API key" : "base URL (leave empty for default)");

            struct json_t *entry = json_object();
            json_set(entry, "description", json_string(desc));
            json_set(entry, "prompt", json_string(prm));
            const char *signup = getstr(pp, "signup_url");
            json_set(entry, "url", *signup ? json_string(signup) : json_null());
            json_set(entry, "password", json_bool(is_key ? true : false));
            json_set(entry, "category", json_string("provider"));
            json_set(entry, "advanced", json_bool(true));
            json_set(entries, var, entry);
        }
    }
    return entries;
}
