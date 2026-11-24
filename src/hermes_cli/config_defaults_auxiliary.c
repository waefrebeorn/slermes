/*
 * config_defaults_auxiliary.c -- real C11 port of hermes_cli/config_defaults_auxiliary.py.
 *
 * PoP: w22_configdefa_fa2045_aux @ hermes_cli/config_defaults_auxiliary.py:_aux
 *
 * The Python module is a single pure factory (`_aux`), so nothing is left
 * upstream: the standard auxiliary block, the optional reasoning_effort key,
 * and the trailing **extra overrides are all reproduced here.
 */
#include "hermes_json.h"
#include "noc_w22_configdefa_fa2045.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *cda_aux_failed(const char *error) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(error ? error : "TypeError"));
    return out;
}

static json_t *cda_aux_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", value ? value : json_null());
    return out;
}

json_t *w22_configdefa_fa2045_aux(const json_t *timeout, int reasoning_effort,
                                  const json_t *extra) {
    json_t *d;
    size_t i;

    /* `**extra` needs a mapping; anything else is a TypeError at the call site. */
    if (extra && extra->type != JSON_NULL && extra->type != JSON_OBJECT) {
        return cda_aux_failed("TypeError");
    }
    /* `_aux(t, reasoning_effort=flag, **extra)` binds a keyword twice when extra
     * re-uses a parameter name -- "timeout" (already positional) or
     * "reasoning_effort" (already a keyword). */
    if (extra && extra->type == JSON_OBJECT &&
        (json_obj_get(extra, "timeout") != NULL ||
         json_obj_get(extra, "reasoning_effort") != NULL)) {
        return cda_aux_failed("TypeError");
    }

    d = json_object();
    json_set(d, "provider", json_string("auto"));
    json_set(d, "model", json_string(""));
    json_set(d, "base_url", json_string(""));
    json_set(d, "api_key", json_string(""));
    /* `"timeout": timeout` keeps whatever object was handed in (None included). */
    json_set(d, "timeout", timeout ? json_copy(timeout) : json_null());
    json_set(d, "extra_body", json_object());
    if (reasoning_effort) {
        json_set(d, "reasoning_effort", json_string(""));
    }
    /* d.update(extra): later keys win, new keys append in order. */
    if (extra && extra->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(extra); i++) {
            const char *key = json_object_get_key_at(extra, i);
            json_t *value = json_object_get_at(extra, i);
            if (!key) continue;
            json_set(d, key, value ? json_copy(value) : json_null());
        }
    }
    return cda_aux_ok(d);
}
