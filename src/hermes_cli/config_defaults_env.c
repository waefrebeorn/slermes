/*
 * config_defaults_env.c — C home for upstream hermes_cli/config_defaults_env.py.
 *
 * PoP: w18_configdefa_a05e90_env @ hermes_cli/config_defaults_env.py:_env
 * PoP: w18_configdefa_a05e90_category @ hermes_cli/config_defaults_env.py:_category
 * PoP: w18_configdefa_a05e90_base_url @ hermes_cli/config_defaults_env.py:_base_url
 *
 * Self-contained, libjson-only.  The threading/locking around OptionalEnvVars
 * is not part of these pure entry-factory functions.
 */
#include "noc_w18_configdefa_a05e90.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int jtruth(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}
static const struct json_t *opt_get(const struct json_t *opts, const char *key) {
    if (!opts || opts->type != JSON_OBJECT) return NULL;
    return json_obj_get(opts, key);
}

struct json_t *w18_configdefa_a05e90_env(const char *description, const char *prompt,
                                         const struct json_t *keys) {
    struct json_t *out = json_object();
    json_set(out, "description", json_string(description ? description : ""));
    json_set(out, "prompt", json_string(prompt ? prompt : ""));
    if (keys && keys->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(keys); ++i) {
            const char *k = json_object_get_key_at(keys, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(keys, i)));
        }
    }
    return out;
}

struct json_t *w18_configdefa_a05e90_category(const char *category,
    int password_default_code, int advanced_default,
    const char *description, const char *prompt, const struct json_t *opts) {
    struct json_t *out = json_object();
    json_set(out, "description", json_string(description ? description : ""));
    json_set(out, "prompt", json_string(prompt ? prompt : ""));

    /* help, url, tools: only when present (even a present null is written). */
    static const char *ordered[] = {"help", "url", "tools"};
    for (size_t i = 0; i < 3; ++i) {
        const struct json_t *v = opt_get(opts, ordered[i]);
        if (v) json_set(out, ordered[i], json_copy(v));
    }

    /* password: effective value = opts override if present else closure default.
     * Written when it is not None. */
    const struct json_t *pw = opt_get(opts, "password");
    int omit_pw = 0;
    struct json_t *pw_val = NULL;
    if (pw) {
        if (pw->type == JSON_NULL) omit_pw = 1;
        else pw_val = json_copy(pw);
    } else {
        if (password_default_code < 0) omit_pw = 1;
        else pw_val = json_bool(password_default_code != 0);
    }
    if (!omit_pw && pw_val) json_set(out, "password", pw_val);
    else if (pw_val) json_free(pw_val);

    json_set(out, "category", json_string(category ? category : ""));

    /* advanced: written as literal True only when effective value is truthy. */
    const struct json_t *adv = opt_get(opts, "advanced");
    int adv_truthy = adv ? jtruth(adv) : (advanced_default != 0);
    if (adv_truthy) json_set(out, "advanced", json_bool(true));

    return out;
}

struct json_t *w18_configdefa_a05e90_base_url(const char *name, const char *prompt_name) {
    const char *nm = name ? name : "";
    const char *pn = (prompt_name && *prompt_name) ? prompt_name : nm;

    char desc[512], prm[640];
    snprintf(desc, sizeof(desc), "%s base URL override", nm);
    snprintf(prm, sizeof(prm), "%s base URL (leave empty for default)", pn);

    /* _prov = _category("provider", password=True, advanced=True), called as
     * _prov(desc, prm, None, password=False): url is present-null. */
    struct json_t *opts = json_object();
    json_set(opts, "url", json_null());
    json_set(opts, "password", json_bool(false));
    struct json_t *out = w18_configdefa_a05e90_category(
        "provider", 1, 1, desc, prm, opts);
    json_free(opts);
    return out;
}
