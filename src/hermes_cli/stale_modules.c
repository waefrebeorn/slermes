/*
 * stale_modules.c — faithful C11 port of hermes_cli/stale_modules.py.
 * Gate w11_stalemodul_bf7d5c.
 *
 * PoP: w11_stalemodul_bf7d5c_drop_stale_root_modules @ hermes_cli/stale_modules.py:drop_stale_root_modules
 */
#include "hermes_json.h"
#include "noc_w11_stalemodul_bf7d5c.h"
#include <string.h>

/* hasattr(module, attr) over the injected attribute list. */
static int has_attr(const json_t *present, const char *attr) {
    if (!present || present->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(present); i++) {
        json_t *v = json_get(present, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, attr)) return 1;
    }
    return 0;
}

/* One (name, attrs) check from checks.items(). */
static void consider(const json_t *modules, const char *name,
                     const json_t *attrs, json_t *out) {
    if (!name) return;
    json_t *present = json_obj_get(modules, name);
    if (!present || present->type == JSON_NULL) return; /* sys.modules.get(name) is None */
    if (!attrs || attrs->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(attrs); i++) {
        json_t *a = json_get(attrs, i);
        const char *an = (a && a->type == JSON_STRING) ? a->str_val : NULL;
        if (!an) continue;
        if (!has_attr(present, an)) {
            json_append(out, json_string(name));
            return;
        }
    }
}

json_t *w11_stalemodul_bf7d5c_drop_stale_root_modules(const json_t *required,
                                                      const json_t *modules) {
    json_t *out = json_array();
    if (required && required->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(required); i++) {
            const char *k = json_object_get_key_at(required, i);
            consider(modules, k, json_object_get_at(required, i), out);
        }
    } else {
        /* Module default: _ROOT_MODULE_REQUIRED_ATTRS. */
        json_t *attrs = json_array();
        json_append(attrs, json_string("file_signature"));
        json_append(attrs, json_string("file_cache_version"));
        consider(modules, "utils", attrs, out);
        json_free(attrs);
    }
    return out;
}
