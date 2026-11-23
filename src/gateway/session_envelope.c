/*
 * session_envelope.c — faithful C11 port of gateway/session_envelope.py.
 * Gate w10_sessionenv_1ffefb.
 *
 * PoP: w10_sessionenv_1ffefb_media_clause @ gateway/session_envelope.py:_media_clause
 * PoP: w10_sessionenv_1ffefb_native_metadata @ gateway/session_envelope.py:_native_metadata
 */
#include "hermes_json.h"
#include "noc_w10_sessionenv_1ffefb.h"
#include <stdlib.h>
#include <string.h>

#define NATIVE_MAX_BYTES (16 * 1024)

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static int is_str_list(const json_t *v) {
    if (!v || v->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < v->c.count; i++)
        if (!v->c.items[i] || v->c.items[i]->type != JSON_STRING) return 0;
    return 1;
}

/* _media_clause: first failing clause name, else NULL (Python None). */
json_t *w10_sessionenv_1ffefb_media_clause(const json_t *event) {
    json_t *urls    = json_obj_get(event, "media_urls");
    json_t *types   = json_obj_get(event, "media_types");
    json_t *inlined = json_obj_get(event, "media_text_inlined");
    json_t *names   = json_obj_get(event, "media_filenames");

    if (!is_str_list(urls)) return json_string("media_urls");
    if (!is_str_list(types)) return json_string("media_types");
    if (!inlined || inlined->type != JSON_ARRAY) return json_string("media_text_inlined");
    for (size_t i = 0; i < inlined->c.count; i++) {
        json_t *it = inlined->c.items[i];
        /* item is None or type(item) is bool (exact bool, not a number). */
        if (!it || !(it->type == JSON_NULL || it->type == JSON_BOOL))
            return json_string("media_text_inlined");
    }
    size_t nurl = urls->c.count;
    if (types->c.count != 0 && types->c.count != nurl)
        return json_string("media_types_length");
    if (inlined->c.count != 0 && inlined->c.count != nurl)
        return json_string("media_text_inlined_length");
    if (!is_str_list(names)) return json_string("media_filenames");
    if (names->c.count != 0 && names->c.count != nurl)
        return json_string("media_filenames_length");
    return json_null();
}

static int key_in(const char *key, const char *const *allowed, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(key, allowed[i])) return 1;
    return 0;
}

/* _native_metadata: JSON projection of kept adapter metadata, NULL = refuse. */
json_t *w10_sessionenv_1ffefb_native_metadata(const struct json_t *event) {
    json_t *metadata = json_obj_get(event, "metadata");
    /* ``if not metadata: return {}`` */
    if (!py_truthy(metadata)) return json_object();
    if (!metadata || metadata->type != JSON_OBJECT) return NULL;
    const char *platform = json_get_str(event, "platform", "");
    if (strcmp(platform, "whatsapp") != 0) return NULL;

    static const char *const meta_keys[] = {
        "whatsapp_native_type", "whatsapp_native", "whatsapp_from_owner"
    };
    static const char *const native_keys[] = {
        "video", "audio", "sticker", "location", "contact", "contacts",
        "reaction", "poll", "pollUpdate"
    };
    for (size_t i = 0; i < json_object_size(metadata); i++) {
        const char *k = json_object_get_key_at(metadata, i);
        if (!k || !key_in(k, meta_keys, sizeof(meta_keys) / sizeof(meta_keys[0]))) return NULL;
    }

    json_t *kind = json_obj_get(metadata, "whatsapp_native_type");
    if (kind && kind->type != JSON_NULL) {
        if (kind->type != JSON_STRING || !kind->str_val) return NULL;
        size_t n = strlen(kind->str_val);
        if (n == 0 || n > 64) return NULL;
    }
    json_t *owner = json_obj_get(metadata, "whatsapp_from_owner");
    if (owner) {
        /* Python: value not in (None, True) -- note 1 == True in Python. */
        int ok = (owner->type == JSON_NULL) ||
                 (owner->type == JSON_BOOL && owner->bool_val) ||
                 (owner->type == JSON_NUMBER && owner->num_val == 1.0);
        if (!ok) return NULL;
    }
    json_t *native = json_obj_get(metadata, "whatsapp_native");
    if (native && native->type != JSON_NULL) {
        if (native->type != JSON_OBJECT) return NULL;
        for (size_t i = 0; i < json_object_size(native); i++) {
            const char *k = json_object_get_key_at(native, i);
            if (!k || !key_in(k, native_keys, sizeof(native_keys) / sizeof(native_keys[0]))) return NULL;
        }
    }

    json_t *projected = json_object();
    if (kind) json_set(projected, "whatsapp_native_type", json_copy(kind));
    if (owner) json_set(projected, "whatsapp_from_owner", json_copy(owner));
    if (native && native->type != JSON_NULL) {
        char *encoded = json_dumps(native, 0);
        size_t nbytes = encoded ? strlen(encoded) : 0;
        if (encoded) free(encoded);
        if (nbytes <= NATIVE_MAX_BYTES)
            json_set(projected, "whatsapp_native", json_copy(native));
    }
    return projected;
}
