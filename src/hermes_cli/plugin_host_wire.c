/*
 * plugin_host_wire.c — real C11 port of the observable value/error wire helpers
 * of upstream Hermes hermes_cli/plugin_host_wire.py (gate w15_pluginhost_60b0ea).
 *
 * The live module's error_payload / raise_remote / decode are pure functions
 * over exception metadata and JSON values; their behaviour is reproduced here
 * without the process/channel machinery (which is I/O, not parity surface).
 *
 * PoP: w15_pluginhost_60b0ea_error_payload @ hermes_cli/plugin_host_wire.py:error_payload
 * PoP: w15_pluginhost_60b0ea_raise_remote @ hermes_cli/plugin_host_wire.py:raise_remote
 * PoP: w15_pluginhost_60b0ea_decode @ hermes_cli/plugin_host_wire.py:decode
 */
#include "noc_w15_pluginhost_60b0ea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _REMOTE_EXCEPTIONS keys from the live module. */
static const char *const k_remote[] = {
    "ValueError", "TypeError", "KeyError", "FileNotFoundError", "PermissionError",
    "NotImplementedError", "TimeoutError", "LookupError", "RuntimeError",
    "PluginHostUnsupported", NULL
};

static int is_remote(const char *name) {
    if (!name) return 0;
    for (int i = 0; k_remote[i]; i++)
        if (!strcmp(name, k_remote[i])) return 1;
    return 0;
}

/* Nearest class in _REMOTE_EXCEPTIONS strictly above the named class in the
 * MRO, excluding RuntimeError (exactly the live generator's rule). Classes that
 * are themselves remote produce no separate "base" entry. */
static const char *remote_base(const char *type_name) {
    if (!type_name) return NULL;
    if (!strcmp(type_name, "JSONDecodeError")) return "ValueError";  /* JSONDecodeError -> ValueError */
    if (!strcmp(type_name, "IndexError")) return "LookupError";      /* IndexError -> LookupError */
    return NULL;
}

json_t *w15_pluginhost_60b0ea_error_payload(const char *type_name, const char *message) {
    const char *t = type_name ? type_name : "";
    const char *m = message ? message : "";
    json_t *out = json_object();
    json_set(out, "type", json_string(t));
    json_set(out, "message", json_string(m));
    const char *base = remote_base(t);
    if (base && strcmp(base, t) != 0)
        json_set(out, "base", json_string(base));
    return out;
}

json_t *w15_pluginhost_60b0ea_raise_remote(const char *type_name, const char *message,
                                          const char *base) {
    const char *t = type_name ? type_name : "";
    const char *m = message ? message : "";
    const char *cls = NULL;
    const char *prefixed = NULL;
    json_t *out = json_object();
    char *buf = NULL;
    int do_prefix = 0;

    if (is_remote(t)) {
        cls = t;
        prefixed = m;
    } else if (is_remote(base)) {
        cls = base;
        do_prefix = 1;
    } else {
        cls = "PluginHostError";
        do_prefix = 1;
    }
    if (do_prefix) {
        if (t[0]) {
            size_t n = strlen(t) + strlen(m) + 3; /* ':' ' ' NUL */
            buf = (char *)malloc(n);
            if (buf) snprintf(buf, n, "%s: %s", t, m);
            prefixed = buf ? buf : m;
        } else {
            prefixed = m;
        }
    }
    json_set(out, "cls", json_string(cls));
    json_set(out, "message", json_string(prefixed ? prefixed : ""));
    free(buf);
    return out;
}

/* Decode a borrowed json_t value into a fresh caller-owned tree. */
json_t *w15_pluginhost_60b0ea_decode(json_t *value) {
    if (!value) return json_null();
    if (value->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(value); i++)
            json_append(out, w15_pluginhost_60b0ea_decode(json_get(value, i)));
        return out;
    }
    if (value->type != JSON_OBJECT) return json_copy(value);

    json_t *r = json_obj_get(value, "__record__");
    if (r) {
        json_t *fields = json_obj_get(value, "fields");
        if (!fields || fields->type == JSON_NULL)
            return json_object();
        return w15_pluginhost_60b0ea_decode(fields);
    }
    json_t *op = json_obj_get(value, "__opaque__");
    if (op) {
        const char *type_text = (op->type == JSON_STRING && op->str_val) ? op->str_val : "";
        size_t n = strlen(type_text) + strlen("<opaque >") + 1;
        char *buf = (char *)malloc(n);
        if (!buf) return json_null();
        snprintf(buf, n, "<opaque %s>", type_text);
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(value); i++) {
        const char *k = json_object_get_key_at(value, i);
        if (!k) continue;
        json_set(out, k, w15_pluginhost_60b0ea_decode(json_object_get_at(value, i)));
    }
    return out;
}
