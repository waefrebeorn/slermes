/*
 * profile_roster_cache.c - C11 port of upstream tui_gateway/profile_roster_cache.py
 * (gate w26_profileros_c45dc1).
 *
 * PoP: w26_profileros_c45dc1_store_signature @ tui_gateway/profile_roster_cache.py:store_signature
 * PoP: w26_profileros_c45dc1_yaml_signature @ tui_gateway/profile_roster_cache.py:profile_yaml_signature
 * PoP: w26_profileros_c45dc1_cached @ tui_gateway/profile_roster_cache.py:_cached
 * PoP: w26_profileros_c45dc1_constants @ tui_gateway/profile_roster_cache.py:<_MAX_ENTRIES, _STORE_FILES>
 *
 * The Python module's I/O seams are `Path.stat()` and `time.time_ns()`. Both are
 * fixed inputs here: a request carries each file's (name, mtime_ns, size, inode)
 * and the clock reading, so what is ported is exactly the decision the stat pair
 * drives -- which files make the signature, when a racy mtime disables the memo,
 * and how the bounded memo reuses a computed field set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_profileros_c45dc1.h"

/* _STORE_FILES */
static const char *const PRC_STORE_FILES[W26_PROFILEROS_C45DC1_STORE_FILES] = {
    "state.db", "state.db-wal",
};

/* _MAX_ENTRIES */
#define PRC_MAX_ENTRIES 512

/* utils/files.py: RACY_MTIME_WINDOW_NS */
#define PRC_RACY_WINDOW_NS 2000000000.0

/* Python truthiness of a JSON value. */
static int prc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python '==' over the shapes a signature element can hold: absent == null. */
static int prc_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!prc_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !prc_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* `files` entry looked up by name; NULL when the file does not exist. */
static const json_t *prc_file(const json_t *files, const char *name) {
    size_t i, n = json_len(files);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(files, i);
        const char *en;
        if (!e || e->type != JSON_OBJECT) continue;
        en = json_get_str(e, "name", NULL);
        if (!en || strcmp(en, name) != 0) continue;
        if (json_obj_get(e, "exists") && !prc_truthy(json_obj_get(e, "exists"))) return NULL;
        return e;
    }
    return NULL;
}

static json_t *prc_part(const json_t *e, int with_inode) {
    json_t *row = json_array();
    json_append(row, json_string(json_get_str(e, "name", "")));
    json_append(row, json_number(json_get_num(e, "mtime_ns", 0.0)));
    json_append(row, json_number(json_get_num(e, "size", 0.0)));
    if (with_inode) json_append(row, json_number(json_get_num(e, "inode", 0.0)));
    return row;
}

json_t *w26_profileros_c45dc1_store_signature(const json_t *req) {
    const json_t *files = json_obj_get(req, "files");
    double now = json_get_num(req, "now_ns", 0.0);
    json_t *parts[W26_PROFILEROS_C45DC1_STORE_FILES];
    double mtimes[W26_PROFILEROS_C45DC1_STORE_FILES];
    size_t have = 0, i;
    json_t *out;

    if (!files || files->type != JSON_ARRAY) return json_null();
    for (i = 0; i < W26_PROFILEROS_C45DC1_STORE_FILES; i++) {
        const json_t *e = prc_file(files, PRC_STORE_FILES[i]);
        if (!e) continue;
        /* An empty -wal sidecar (our own read-only open creates one) is not a move. */
        if (json_get_num(e, "size", 0.0) == 0.0 &&
            strcmp(json_get_str(e, "name", ""), "state.db-wal") == 0)
            continue;
        mtimes[have] = json_get_num(e, "mtime_ns", 0.0);
        parts[have++] = prc_part(e, 0);
    }
    if (have == 0) return json_null();
    for (i = 0; i < have; i++) {
        /* mtime_is_racy: now - mtime_ns < RACY_MTIME_WINDOW_NS */
        if (now - mtimes[i] < PRC_RACY_WINDOW_NS) {
            size_t j;
            for (j = 0; j < have; j++) json_free(parts[j]);
            return json_null();
        }
    }
    out = json_array();
    for (i = 0; i < have; i++) json_append(out, parts[i]);
    return out;
}

json_t *w26_profileros_c45dc1_yaml_signature(const json_t *req) {
    const json_t *files = json_obj_get(req, "files");
    const json_t *e;
    if (!files || files->type != JSON_ARRAY) return json_null();
    e = prc_file(files, "profile.yaml");
    if (!e) return json_null();
    return prc_part(e, 1);
}

/* The memo: one ordered list of (key, signature, fields) entries. */
typedef struct {
    const json_t *key;       /* borrowed from the request */
    const json_t *signature; /* borrowed */
    const json_t *fields;    /* borrowed */
} prc_entry;

static json_t *prc_state_rows(const prc_entry *entries, size_t n) {
    json_t *rows = json_array();
    size_t i;
    for (i = 0; i < n; i++) {
        json_t *row = json_object();
        json_set(row, "key", entries[i].key ? json_copy(entries[i].key) : json_null());
        json_set(row, "signature",
                 entries[i].signature ? json_copy(entries[i].signature) : json_null());
        json_set(row, "fields",
                 json_copy(entries[i].fields ? entries[i].fields : json_object()));
        json_append(rows, row);
    }
    return rows;
}

json_t *w26_profileros_c45dc1_cached(const json_t *req) {
    const json_t *cache = json_obj_get(req, "cache");
    const json_t *key = json_obj_get(req, "key");
    const json_t *sig = json_obj_get(req, "signature");
    const json_t *fields = json_obj_get(req, "fields");
    double max_entries = json_get_num(req, "max_entries", (double)PRC_MAX_ENTRIES);
    prc_entry *entries;
    size_t n = 0, i, found = (size_t)-1;
    int hit = 0;
    json_t *out;

    if (!cache || cache->type != JSON_ARRAY) return json_null();
    entries = (prc_entry *)calloc(json_len(cache) + 1, sizeof(prc_entry));
    if (!entries) return json_null();
    for (i = 0; i < json_len(cache); i++) {
        const json_t *e = json_get(cache, i);
        const json_t *k;
        if (!e || e->type != JSON_OBJECT) continue;
        k = json_obj_get(e, "key");
        if (!k) continue;
        entries[n].key = k;
        entries[n].signature = json_obj_get(e, "signature");
        entries[n].fields = json_obj_get(e, "fields");
        n++;
    }

    out = json_object();
    /* No signature means no cache: compute() and leave the memo untouched. */
    if (!sig || sig->type == JSON_NULL) {
        json_set(out, "fields", json_copy(fields ? fields : json_object()));
        json_set(out, "cache", prc_state_rows(entries, n));
        free(entries);
        return out;
    }

    for (i = 0; i < n; i++) {
        if (prc_equal(entries[i].key, key)) { found = i; break; }
    }
    if (found != (size_t)-1 && prc_equal(entries[found].signature, sig)) {
        hit = 1; /* copier(hit[1]): a fresh copy of the memoized fields */
        json_set(out, "fields",
                 json_copy(entries[found].fields ? entries[found].fields : json_object()));
    } else {
        json_set(out, "fields", json_copy(fields ? fields : json_object()));
    }
    if (!hit) {
        if (n >= (size_t)max_entries) { n = 0; found = (size_t)-1; } /* cache.clear() */
        if (found != (size_t)-1) {
            entries[found].signature = sig;
            entries[found].fields = fields; /* position preserved: dict update */
        } else {
            entries[n].key = key;
            entries[n].signature = sig;
            entries[n].fields = fields;
            n++;
        }
    }
    json_set(out, "cache", prc_state_rows(entries, n));
    free(entries);
    return out;
}

json_t *w26_profileros_c45dc1_constants(void) {
    json_t *o = json_object();
    json_t *files = json_array();
    size_t i;
    for (i = 0; i < W26_PROFILEROS_C45DC1_STORE_FILES; i++)
        json_append(files, json_string(PRC_STORE_FILES[i]));
    json_set(o, "max_entries", json_number((double)PRC_MAX_ENTRIES));
    json_set(o, "store_files", files);
    return o;
}
