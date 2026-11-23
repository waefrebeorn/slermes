/*
 * canonical.c - Canonical JSON encoding (C11 port).
 *
 * PoP: canon_canonical_dumps @ persistence/core/canonical.py:canonical_dumps
 * PoP: canon_canonical_json @ persistence/core/canonical.py:canonical_json
 *
 * Mirrors json.dumps(ensure_ascii=False, sort_keys=True,
 * separators=(",", ":"), allow_nan=False): libjson's serializer already emits
 * compact separators, keeps non-ASCII as UTF-8 bytes and escapes control chars
 * exactly as Python does, so the port only needs a recursive key sort before
 * serializing.  canonical_json refuses non-objects with AuthorityRefusal.
 */
#include "noc_canon.h"

#include <stdlib.h>
#include <string.h>

static json_t *canon_sorted_tree(const json_t *n) {
    if (!n) return json_null();
    if (n->type == JSON_OBJECT) {
        size_t cnt = json_object_size(n);
        size_t *idx = (size_t *)malloc((cnt ? cnt : 1) * sizeof(size_t));
        for (size_t i = 0; i < cnt; i++) idx[i] = i;
        for (size_t i = 1; i < cnt; i++) {
            size_t cur = idx[i];
            const char *ck = json_object_get_key_at(n, cur);
            size_t j = i;
            while (j > 0 && strcmp(json_object_get_key_at(n, idx[j - 1]), ck) > 0) {
                idx[j] = idx[j - 1]; j--;
            }
            idx[j] = cur;
        }
        json_t *o = json_object();
        for (size_t k = 0; k < cnt; k++) {
            size_t i = idx[k];
            json_set(o, json_object_get_key_at(n, i),
                     canon_sorted_tree(json_object_get_at(n, i)));
        }
        free(idx);
        return o;
    }
    if (n->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < json_len(n); i++) json_append(a, canon_sorted_tree(json_get(n, i)));
        return a;
    }
    return json_copy(n);
}

char *canon_canonical_dumps(const json_t *value) {
    if (!value) return strdup("null");
    json_t *sorted = canon_sorted_tree(value);
    char *s = json_serialize(sorted);
    json_free(sorted);
    return s;
}

char *canon_canonical_json(const json_t *value, int *refused) {
    if (refused) *refused = 0;
    if (!value || value->type != JSON_OBJECT) {
        if (refused) *refused = 1;
        return NULL;
    }
    return canon_canonical_dumps(value);
}
