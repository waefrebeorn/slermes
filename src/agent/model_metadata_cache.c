/*
 * model_metadata_cache.c — C port of upstream Hermes agent/model_metadata_cache.py.
 *
 * PoP: w13_modelmetad_9e75ab_drop_context_cache_rows @ agent/model_metadata_cache.py:drop_context_cache_rows
 *
 * The lock and atomic YAML write are seams (see header): the oracle stubs
 * hermes_cli.auth._file_lock to succeed and captures the document handed to
 * utils.atomic_yaml_write. This translation unit models the load -> mutate ->
 * write decision of drop_context_cache_rows exactly, returning the persisted
 * document (or JSON null when the update is skipped as a no-op).
 */
#include "noc_w13_modelmetad_9e75ab.h"
#include "hermes_json.h"
#include <string.h>

/* Python truthiness of a JSON value (used by `... or {}` and `if confirmed:`). */
static int jtruthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

static int dict_has(const json_t *d, const char *k) {
    return d && d->type == JSON_OBJECT && json_obj_get(d, k) != NULL;
}

/* Copy an object, dropping every key listed in stale_keys (dict.pop(k, None)). */
static json_t *drop_keys(const json_t *obj, const json_t *stale) {
    json_t *out = json_object();
    if (!json_is_object(obj)) return out;
    for (size_t i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        json_t *v = json_object_get_at(obj, i);
        if (!k) continue;
        int stale_k = 0;
        if (json_is_array(stale)) {
            for (size_t j = 0; j < json_len(stale); j++) {
                json_t *s = json_get(stale, j);
                if (s && s->type == JSON_STRING && !strcmp(s->str_val, k)) { stale_k = 1; break; }
            }
        }
        if (!stale_k) json_set(out, k, json_copy(v));
    }
    return out;
}

struct json_t *w13_modelmetad_9e75ab_drop_context_cache_rows(const json_t *document,
                                                             const json_t *stale_keys) {
    /* cache = document.get("context_lengths") or {}
     * confirmed = document.get("bedrock_confirmed_v1") or {}
     * confirmed = confirmed if isinstance(confirmed, dict) else {} */
    json_t *cache_src = json_is_object(document) ? json_obj_get(document, "context_lengths") : NULL;
    json_t *cache = (jtruthy(cache_src) && cache_src->type == JSON_OBJECT) ? cache_src : NULL;
    json_t *conf_src = json_is_object(document) ? json_obj_get(document, "bedrock_confirmed_v1") : NULL;
    json_t *confirmed = (jtruthy(conf_src) && conf_src->type == JSON_OBJECT) ? conf_src : NULL;

    /* if not any(k in cache or k in confirmed for k in stale_keys): return */
    int present = 0;
    if (json_is_array(stale_keys)) {
        for (size_t i = 0; i < json_len(stale_keys); i++) {
            json_t *k = json_get(stale_keys, i);
            if (!k || k->type != JSON_STRING) continue;
            if (dict_has(cache, k->str_val) || dict_has(confirmed, k->str_val)) { present = 1; break; }
        }
    }
    if (!present) return json_null();  /* no write -> wrapper returns None */

    json_t *new_cache = drop_keys(cache, stale_keys);
    json_t *new_conf = drop_keys(confirmed, stale_keys);
    json_t *written = json_object();
    json_set(written, "context_lengths", new_cache);
    /* _write_context_cache adds bedrock_confirmed_v1 only when truthy. */
    if (jtruthy(new_conf))
        json_set(written, "bedrock_confirmed_v1", new_conf);
    else
        json_free(new_conf);
    return written;
}
