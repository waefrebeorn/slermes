/*
 * plugin.c - C home for agent/vault_backends/plugin.py.
 * Core-owned wrapper around a plugin-registered login backend: namespace ownership,
 * the fixed-text failure sentence, and the validated copy of plugin-returned metadata.
 * PoP: w19_plugin_a94384_text @ agent/vault_backends/plugin.py:_text
 * PoP: w19_plugin_a94384_materialize @ agent/vault_backends/plugin.py:_materialize
 * PoP: w19_plugin_a94384_owns @ agent/vault_backends/plugin.py:PluginLoginBackend.owns
 * PoP: w19_plugin_a94384_list_items @ agent/vault_backends/plugin.py:PluginLoginBackend.list_items
 * PoP: w19_plugin_a94384_get_meta @ agent/vault_backends/plugin.py:PluginLoginBackend.get_meta
 * PoP: w19_plugin_a94384_resolve_password @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_password
 * PoP: w19_plugin_a94384_resolve_otp @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_otp
 * PoP: w19_plugin_a94384_resolve_secret @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_secret
 * PoP: w19_plugin_a94384_fill_context @ agent/vault_backends/plugin.py:PluginLoginBackend.fill_context
 */
#include "noc_w19_plugin_a94384.h"
#include <stdio.h>
#include <string.h>

static const char *VAULT_KINDS[] = {"login", "payment", "address"};

static json_t *invalid(void) {
    json_t *o = json_object();
    json_set(o, "invalid", json_bool(true));
    return o;
}

static json_t *ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "status", json_string("ok"));
    json_set(o, "value", value);
    return o;
}

static json_t *fail(const char *fmt, const char *name) {
    char buf[512];
    json_t *o = json_object();
    snprintf(buf, sizeof buf, fmt, name ? name : "");
    json_set(o, "status", json_string("error"));
    json_set(o, "message", json_string(buf));
    return o;
}

static json_t *spec(json_t *plugin, const char *verb) {
    json_t *v = plugin && plugin->type == JSON_OBJECT ? json_obj_get(plugin, verb) : NULL;
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static int spec_flag(json_t *s, const char *key) {
    json_t *v = s ? json_obj_get(s, key) : NULL;
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return 0;
}

/* _text(value): str.__str__(value) for an exact str, else _INVALID. */
json_t *w19_plugin_a94384_text(json_t *value) {
    if (!value || value->type != JSON_STRING) return invalid();
    return json_string(value->str_val);
}

static int kind_ok(const char *kind) {
    for (size_t i = 0; i < sizeof VAULT_KINDS / sizeof VAULT_KINDS[0]; i++)
        if (!strcmp(kind, VAULT_KINDS[i])) return 1;
    return 0;
}

/* _materialize(meta): a core-owned VaultItemMeta copied field by field, or _INVALID. */
json_t *w19_plugin_a94384_materialize(json_t *meta) {
    static const char *TEXT_FIELDS[] = {"id", "kind", "label", "created_at"};
    static const char *OPT_FIELDS[] = {"origin", "identifier", "identifier_type"};
    if (!meta || meta->type != JSON_OBJECT) return invalid();
    /* The live code reads an attribute; a missing one raises, so a wrong or absent
     * required field reads invalid here. */
    for (size_t i = 0; i < 4; i++) {
        json_t *v = json_obj_get(meta, TEXT_FIELDS[i]);
        if (!v || v->type != JSON_STRING) return invalid();
    }
    for (size_t i = 0; i < 3; i++) {
        json_t *v = json_obj_get(meta, OPT_FIELDS[i]);
        if (v && v->type != JSON_NULL && v->type != JSON_STRING) return invalid();
    }
    json_t *has_otp = json_obj_get(meta, "has_otp");
    if (!has_otp || has_otp->type != JSON_BOOL) return invalid();
    json_t *origins = json_obj_get(meta, "allowed_origins");
    if (!origins || origins->type != JSON_ARRAY) return invalid();
    /* a list, not a tuple: the live isinstance(origins, tuple) refuses it */
    if (spec_flag(meta, "_origins_list")) return invalid();
    for (size_t i = 0; i < json_len(origins); i++) {
        json_t *o = json_get(origins, i);
        if (!o || o->type != JSON_STRING) return invalid();
    }
    json_t *kind = json_obj_get(meta, "kind");
    if (!kind_ok(kind->str_val)) return invalid();

    json_t *out = json_object();
    for (size_t i = 0; i < 4; i++) {
        json_t *v = json_obj_get(meta, TEXT_FIELDS[i]);
        json_set(out, TEXT_FIELDS[i], json_string(v->str_val));
    }
    for (size_t i = 0; i < 3; i++) {
        json_t *v = json_obj_get(meta, OPT_FIELDS[i]);
        json_set(out, OPT_FIELDS[i],
                 (v && v->type == JSON_STRING) ? json_string(v->str_val) : json_null());
    }
    json_set(out, "has_otp", json_bool(has_otp->bool_val ? true : false));
    json_set(out, "allowed_origins", json_copy(origins));
    return out;
}

/* owns(handle): isinstance(handle, str) and handle.startswith(f"{name}:") */
int w19_plugin_a94384_owns(const char *name, const char *handle) {
    size_t n;
    if (!handle) return 0;
    n = name ? strlen(name) : 0;
    return strncmp(handle, name ? name : "", n) == 0 && handle[n] == ':';
}

static json_t *plugin_value(json_t *s) {
    if (!s) return NULL;
    return json_obj_get(s, "value");
}

json_t *w19_plugin_a94384_list_items(const char *name, json_t *plugin) {
    json_t *s = spec(plugin, "list_items");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    json_t *items = s ? json_obj_get(s, "items") : NULL;
    json_t *out = json_array();
    if (items && items->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(items); i++) {
            json_t *m = w19_plugin_a94384_materialize(json_get(items, i));
            if (spec_flag(m, "invalid")) { json_free(m); json_free(out); return fail("The %s login backend returned an invalid value.", name); }
            json_append(out, m);
        }
    }
    /* return [m for m in items if self.owns(m.id)] */
    json_t *kept = json_array();
    for (size_t i = 0; i < json_len(out); i++) {
        json_t *m = json_get(out, i);
        json_t *id = m ? json_obj_get(m, "id") : NULL;
        if (id && id->type == JSON_STRING && w19_plugin_a94384_owns(name, id->str_val))
            json_append(kept, json_copy(m));
    }
    json_free(out);
    return ok(kept);
}

json_t *w19_plugin_a94384_get_meta(const char *name, json_t *plugin, const char *handle) {
    if (!w19_plugin_a94384_owns(name, handle)) return ok(json_null());
    json_t *s = spec(plugin, "get_meta");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    json_t *raw = plugin_value(s);
    json_t *meta = (raw && raw->type != JSON_NULL) ? w19_plugin_a94384_materialize(raw) : json_null();
    if (meta && meta->type == JSON_OBJECT && spec_flag(meta, "invalid")) {
        json_free(meta);
        return fail("The %s login backend returned an invalid value.", name);
    }
    if (!meta || meta->type == JSON_NULL) {
        json_free(meta);
        return ok(json_null());
    }
    /* return meta if meta.id == handle else None */
    json_t *id = json_obj_get(meta, "id");
    if (!id || id->type != JSON_STRING || strcmp(id->str_val, handle)) {
        json_free(meta);
        return ok(json_null());
    }
    return ok(meta);
}

json_t *w19_plugin_a94384_resolve_password(const char *name, json_t *plugin, const char *handle) {
    if (!w19_plugin_a94384_owns(name, handle))
        return fail("Handle is outside the %s login backend's namespace.", name);
    json_t *s = spec(plugin, "resolve_password");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    json_t *text = w19_plugin_a94384_text(plugin_value(s));
    if (spec_flag(text, "invalid")) { json_free(text); return fail("The %s login backend returned an invalid value.", name); }
    return ok(text);
}

json_t *w19_plugin_a94384_resolve_otp(const char *name, json_t *plugin, const char *handle) {
    if (!w19_plugin_a94384_owns(name, handle))
        return fail("Handle is outside the %s login backend's namespace.", name);
    json_t *s = spec(plugin, "resolve_otp");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    json_t *raw = plugin_value(s);
    if (!raw || raw->type == JSON_NULL) return ok(json_null());
    json_t *text = w19_plugin_a94384_text(raw);
    if (spec_flag(text, "invalid")) { json_free(text); return fail("The %s login backend returned an invalid value.", name); }
    return ok(text);
}

json_t *w19_plugin_a94384_resolve_secret(const char *name, json_t *plugin, const char *handle) {
    if (!w19_plugin_a94384_owns(name, handle))
        return fail("Handle is outside the %s login backend's namespace.", name);
    json_t *s = spec(plugin, "resolve_secret");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    json_t *raw = plugin_value(s);
    if (!raw || raw->type != JSON_OBJECT) return fail("The %s login backend returned an invalid value.", name);
    for (size_t i = 0; i < json_object_size(raw); i++) {
        json_t *v = json_object_get_at(raw, i);
        if (!v || v->type != JSON_STRING) return fail("The %s login backend returned an invalid value.", name);
    }
    return ok(json_copy(raw));
}

json_t *w19_plugin_a94384_fill_context(const char *name, json_t *plugin) {
    json_t *s = spec(plugin, "fill_context");
    if (spec_flag(s, "raise")) return fail("The %s login backend failed.", name);
    if (spec_flag(s, "enter_raise")) return fail("The %s login backend failed.", name);
    if (spec_flag(s, "exit_raise")) return fail("The %s login backend failed.", name);
    return ok(json_bool(true));
}
