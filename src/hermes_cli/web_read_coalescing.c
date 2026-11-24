/*
 * web_read_coalescing.c — C port of upstream Hermes hermes_cli/web_read_coalescing.py.
 *
 * PoP: w18_webreadco_957679_freeze @ hermes_cli/web_read_coalescing.py:_freeze
 *
 * _freeze builds a (type, payload) tuple that distinguishes dict/list/tuple/
 * set/frozenset and recursively freezes children; frozensets sort their
 * members. This port renders that structure canonically as JSON so the live
 * Python result can be compared directly by the oracle.
 */
#include "noc_w18_webreadco_957679.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *frozen(const char *tname, json_t *payload) {
    json_t *o = json_object();
    json_set(o, "t", json_string(tname));
    json_set(o, "v", payload);
    return o;
}

/* Return a copy of arr's elements sorted by json_dumps(bytes) of each element. */
static json_t *sorted_by_dump(json_t *arr) {
    size_t n = json_len(arr);
    if (n > 4096) n = 4096;
    const char *dumps[4096];
    int owned[4096];
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        dumps[i] = e ? json_dumps(e, 0) : "null";
        owned[i] = e ? 1 : 0;
    }
    size_t order[4096];
    for (size_t i = 0; i < n; i++) order[i] = i;
    for (size_t i = 1; i < n; i++) {
        size_t cur = order[i];
        size_t j = i;
        while (j > 0 && strcmp(dumps[order[j - 1]], dumps[cur]) > 0) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = cur;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        json_append(out, json_copy(json_get(arr, order[i])));
        if (owned[i]) free((void *)dumps[i]);
    }
    return out;
}

static json_t *freeze_rec(const json_t *v, int depth) {
    if (!v || depth > 32) return frozen("NoneType", json_null());
    switch (v->type) {
        case JSON_NULL:  return frozen("NoneType", json_null());
        case JSON_BOOL:  return frozen("bool", json_bool(v->bool_val));
        case JSON_STRING: return frozen("str", json_string(v->str_val));
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                return frozen("int", json_number((double)(long long)v->num_val));
            return frozen("float", json_number(v->num_val));
        case JSON_ARRAY: {
            json_t *items = json_array();
            for (size_t i = 0; i < json_len(v); i++)
                json_append(items, freeze_rec(json_get(v, i), depth + 1));
            return frozen("list", items);
        }
        case JSON_OBJECT: {
            static const char *marks[3] = {"__tuple__", "__set__", "__frozenset__"};
            static const char *names[3] = {"tuple", "set", "frozenset"};
            for (int m = 0; m < 3; m++) {
                json_t *mv = json_object_size(v) == 1 ? json_obj_get(v, marks[m]) : NULL;
                if (mv && json_is_array(mv)) {
                    json_t *items = json_array();
                    for (size_t i = 0; i < json_len(mv); i++)
                        json_append(items, freeze_rec(json_get(mv, i), depth + 1));
                    if (m != 0) {
                        json_t *sorted = sorted_by_dump(items);
                        json_free(items);
                        items = sorted;
                    }
                    return frozen(names[m], items);
                }
            }
            json_t *pairs = json_array();
            for (size_t i = 0; i < json_object_size(v); i++) {
                const char *k = json_object_get_key_at(v, i);
                json_t *pair = json_array();
                json_append(pair, freeze_rec(json_string(k ? k : ""), depth + 1));
                json_append(pair, freeze_rec(json_object_get_at(v, i), depth + 1));
                json_append(pairs, pair);
            }
            json_t *sorted = sorted_by_dump(pairs);
            json_free(pairs);
            return frozen("dict", sorted);
        }
        default: return frozen("NoneType", json_null());
    }
}

json_t *w18_webreadco_957679_freeze(const json_t *value) {
    return freeze_rec(value, 0);
}
