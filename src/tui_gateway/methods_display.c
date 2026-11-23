/*
 * methods_display.c - C home for upstream Hermes tui_gateway/methods_display.py.
 * PoP: w15_methodsdis_df8f9b_mint_viewer_id @ tui_gateway/methods_display.py:_mint_viewer_id
 * PoP: w15_methodsdis_df8f9b_foreign_viewer_id @ tui_gateway/methods_display.py:_foreign_viewer_id
 *
 * Faithful port of the viewer-id minting capability check. The live module keeps a
 * WeakKeyDictionary of ids per connection and falls back to a process-wide set for
 * unweakable transports; the fixture supplies that set explicitly (`mine`) so no
 * transport / secrets state leaks into the port. The RPC handlers themselves bind
 * live runtime, lease and renderer state and stay with the live Python owner.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w15_methodsdis_df8f9b.h"

static int strset_has(const json_t *arr, const char *s) {
    if (!s || !arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *it = json_get(arr, i);
        if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, s)) return 1;
    }
    return 0;
}

static int cmp_str_ptr(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

/* Sorted, de-duplicated snapshot of a Python set of strings (set ordering is
 * arbitrary in Python, so the oracle sorts both sides). */
static json_t *strset_sorted(const json_t *mine, const char *extra) {
    size_t n = (mine && mine->type == JSON_ARRAY) ? json_len(mine) : 0;
    const char **v = (const char **)calloc(n + 1, sizeof(const char *));
    size_t k = 0;
    if (v) {
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_get(mine, i);
            if (it && it->type == JSON_STRING && it->str_val) v[k++] = it->str_val;
        }
        if (extra) v[k++] = extra;
        if (k > 1) qsort(v, k, sizeof(const char *), cmp_str_ptr);
        json_t *out = json_array();
        for (size_t i = 0; i < k; i++) {
            if (i && v[i] && v[i - 1] && !strcmp(v[i], v[i - 1])) continue;
            json_append(out, json_string(v[i]));
        }
        free(v);
        return out;
    }
    return json_array();
}

struct json_t *w15_methodsdis_df8f9b_mint_viewer_id(const char *requested,
                                                     const struct json_t *mine,
                                                     const char *generated) {
    int honoured = requested && strset_has(mine, requested);
    const char *id = honoured ? requested : (generated ? generated : "");
    json_t *out = json_object();
    json_set(out, "id", json_string(id));
    json_set(out, "minted", strset_sorted(mine, honoured ? NULL : id));
    return out;
}

struct json_t *w15_methodsdis_df8f9b_foreign_viewer_id(const char *viewer_id,
                                                        const struct json_t *mine) {
    return json_bool(!(viewer_id && strset_has(mine, viewer_id)));
}
