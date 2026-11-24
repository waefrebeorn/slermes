/*
 * registry.c -- real C11 port of the local JSON registry of approved remote
 * meet nodes (plugins/google_meet/node/registry.py).
 *
 * PoP: w21_registry_5bbc35_default_path @ plugins/google_meet/node/registry.py:_default_path
 * PoP: w21_registry_5bbc35_get @ plugins/google_meet/node/registry.py:NodeRegistry.get
 * PoP: w21_registry_5bbc35_add @ plugins/google_meet/node/registry.py:NodeRegistry.add
 * PoP: w21_registry_5bbc35_remove @ plugins/google_meet/node/registry.py:NodeRegistry.remove
 * PoP: w21_registry_5bbc35_list_all @ plugins/google_meet/node/registry.py:NodeRegistry.list_all
 * PoP: w21_registry_5bbc35_resolve @ plugins/google_meet/node/registry.py:NodeRegistry.resolve
 *
 * Faithful details:
 *  - read_json: missing/unreadable/malformed -> None, the text is decoded as
 *    utf-8-sig (a BOM must not wipe the registry);
 *  - _load: `data.get("nodes")` only when the document is a dict, and the
 *    result is used only when it is a dict -- anything else is {};
 *  - add validates in order (node name, url, token) with
 *    "<label> must be a non-empty string";
 *  - list_all/resolve sort by name; resolve without a name returns the sole
 *    node and None when zero or several are registered;
 *  - _save writes {"nodes": nodes} owner-only (mode 0600).
 */
#include "noc_w21_registry_5bbc35.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *rg_dup_n(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (n && s) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *rg_dup(const char *s) {
    if (!s) s = "";
    return rg_dup_n(s, strlen(s));
}

static int rg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- file helpers --------------------------------------------------------- */

static char *rg_slurp(const char *path) {
    FILE *fh;
    char *buf = NULL;
    size_t n = 0, cap = 0;
    if (!path) return NULL;
    fh = fopen(path, "rb");
    if (!fh) return NULL;
    for (;;) {
        size_t got;
        if (n + 4096 + 1 > cap) {
            size_t ncap = cap ? cap * 2 : 8192;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) {
                free(buf);
                fclose(fh);
                return NULL;
            }
            buf = nb;
            cap = ncap;
        }
        got = fread(buf + n, 1, 4096, fh);
        n += got;
        if (got < 4096) break;
    }
    fclose(fh);
    if (!buf) return NULL;
    buf[n] = '\0';
    /* utf-8-sig */
    if (n >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, n - 3 + 1);
    }
    return buf;
}

/* read_json(): parsed JSON or NULL when missing/unreadable/malformed. */
static json_t *rg_read_json(const char *path) {
    struct stat st;
    char *text;
    json_t *out;
    if (!path || stat(path, &st) != 0) return NULL;
    if ((st.st_mode & S_IFMT) != S_IFREG) return NULL;
    text = rg_slurp(path);
    if (!text) return NULL;
    out = json_parse(text, NULL);
    free(text);
    return out;
}

static void rg_mkdir_parents(const char *path) {
    char *copy = rg_dup(path);
    char *p;
    if (!copy) return;
    for (p = copy + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            if (copy[0]) mkdir(copy, 0755);
            *p = sep;
        }
    }
    free(copy);
}

static void rg_chmod_owner_only(const char *path) {
#ifdef _WIN32
    (void)path;   /* Windows ACLs do not map onto the mode bits */
#else
    chmod(path, 0600);
#endif
}

/* atomic_json_write(path, {"nodes": nodes}, mode=0o600) */
static int rg_write_nodes(const char *path, json_t *nodes) {
    json_t *doc;
    char *text;
    FILE *fh;
    int ok = 0;
    if (!path) return 0;
    rg_mkdir_parents(path);
    doc = json_object();
    json_set(doc, "nodes", json_copy(nodes ? nodes : json_object()));
    text = json_dumps(doc, 2);
    json_free(doc);
    if (!text) return 0;
    fh = fopen(path, "wb");
    if (fh) {
        if (fwrite(text, 1, strlen(text), fh) == strlen(text)) ok = 1;
        fclose(fh);
        rg_chmod_owner_only(path);
    }
    free(text);
    return ok;
}

/* --- NodeRegistry._load --------------------------------------------------- */

json_t *w21_registry_5bbc35_load_nodes(const char *path) {
    json_t *data = rg_read_json(path);
    json_t *nodes;
    if (!data) return json_object();
    nodes = (data->type == JSON_OBJECT) ? json_obj_get(data, "nodes") : NULL;
    if (!nodes || nodes->type != JSON_OBJECT) {
        json_free(data);
        return json_object();
    }
    {
        json_t *out = json_copy(nodes);
        json_free(data);
        return out;
    }
}

static json_t *rg_error(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type));
    json_set(out, "message", json_string(message));
    return out;
}

/* `{"name": name, **entry}` raises TypeError for a non-mapping entry. */
static const char *rg_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static json_t *rg_named(const char *name, const json_t *entry) {
    json_t *out = json_object();
    size_t i;
    if (entry && entry->type != JSON_OBJECT) {
        char message[96];
        snprintf(message, sizeof(message), "'%s' object is not a mapping", rg_py_type_name(entry));
        json_free(out);
        return rg_error("TypeError", message);
    }
    json_set(out, "name", json_string(name ? name : ""));
    if (entry) {
        for (i = 0; i < entry->c.count; i++) {
            const char *k = entry->c.keys ? entry->c.keys[i] : NULL;
            if (k) json_set(out, k, json_copy(entry->c.items[i]));
        }
    }
    return out;
}

json_t *w21_registry_5bbc35_get(const char *path, const char *name) {
    json_t *nodes = w21_registry_5bbc35_load_nodes(path);
    json_t *entry = json_obj_get(nodes, name ? name : "");
    json_t *out;
    if (!entry) {
        json_free(nodes);
        return json_null();
    }
    out = rg_named(name ? name : "", entry);
    json_free(nodes);
    return out;
}

static int rg_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w21_registry_5bbc35_list_all(const char *path) {
    json_t *nodes = w21_registry_5bbc35_load_nodes(path);
    json_t *out = json_array();
    const char **keys;
    size_t n = json_object_size(nodes), i;

    keys = (const char **)calloc(n ? n : 1, sizeof(*keys));
    if (!keys) {
        json_free(nodes);
        return out;
    }
    for (i = 0; i < n; i++) keys[i] = json_object_get_key_at(nodes, i);
    if (n) qsort(keys, n, sizeof(keys[0]), rg_cmp_str);
    for (i = 0; i < n; i++) {
        json_t *named = rg_named(keys[i], json_obj_get(nodes, keys[i]));
        if (named && named->type == JSON_OBJECT && json_obj_get(named, "status")) {
            /* a non-dict entry raises out of the comprehension */
            json_free(out);
            free(keys);
            json_free(nodes);
            return named;
        }
        json_append(out, named);
    }
    free(keys);
    json_free(nodes);
    return out;
}

json_t *w21_registry_5bbc35_remove(const char *path, const char *name) {
    json_t *nodes = w21_registry_5bbc35_load_nodes(path);
    json_t *entry = json_obj_get(nodes, name ? name : "");
    json_t *kept;
    int ok = 0;
    size_t i;
    if (!entry) {
        json_free(nodes);
        return json_bool(0);
    }
    kept = json_object();
    for (i = 0; i < json_object_size(nodes); i++) {
        const char *k = json_object_get_key_at(nodes, i);
        if (!k || !strcmp(k, name ? name : "")) continue;
        json_set(kept, k, json_copy(json_object_get_at(nodes, i)));
    }
    ok = rg_write_nodes(path, kept);
    json_free(kept);
    json_free(nodes);
    return json_bool(ok ? 1 : 0);
}

/* The raw registry document as the file holds it (null when missing or
 * malformed) -- what the gate reports after a write. */
json_t *w21_registry_5bbc35_read_document(const char *path) {
    json_t *doc = rg_read_json(path);
    return doc ? doc : json_null();
}

json_t *w21_registry_5bbc35_add(const char *path, const json_t *name,
                               const json_t *url, const json_t *token, double now) {
    static const char *const LABELS[] = {"node name", "url", "token"};
    const json_t *values[3];
    json_t *nodes, *entry, *out;
    char message[128];
    int i;

    values[0] = name;
    values[1] = url;
    values[2] = token;
    for (i = 0; i < 3; i++) {
        const json_t *v = values[i];
        if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) {
            snprintf(message, sizeof(message), "%s must be a non-empty string", LABELS[i]);
            return rg_error("ValueError", message);
        }
    }

    nodes = w21_registry_5bbc35_load_nodes(path);
    entry = json_object();
    json_set(entry, "url", json_string(url->str_val));
    json_set(entry, "token", json_string(token->str_val));
    json_set(entry, "added_at", json_number(now));
    json_set(nodes, name->str_val, entry);

    if (!rg_write_nodes(path, nodes)) {
        json_free(nodes);
        return rg_error("OSError", "registry write failed");
    }
    json_free(nodes);
    out = json_object();
    json_set(out, "status", json_string("ok"));
    return out;
}

json_t *w21_registry_5bbc35_resolve(const char *path, const json_t *chrome_node) {
    if (rg_truthy(chrome_node)) {
        const char *name = (chrome_node->type == JSON_STRING) ? chrome_node->str_val : "";
        return w21_registry_5bbc35_get(path, name);
    }
    {
        json_t *all = w21_registry_5bbc35_list_all(path);
        json_t *first = NULL;
        if (all && all->type == JSON_ARRAY && all->c.count == 1) first = json_copy(all->c.items[0]);
        json_free(all);
        return first ? first : json_null();
    }
}

json_t *w21_registry_5bbc35_default_path(const json_t *hermes_home_env) {
    const char *home = NULL;
    size_t len;
    char *out;
    if (!hermes_home_env || hermes_home_env->type != JSON_STRING ||
        !hermes_home_env->str_val || !hermes_home_env->str_val[0]) {
        /* Hermes-home resolution itself is hermes_constants' job (the seam);
         * with no injected value there is no path to answer with. */
        return json_null();
    }
    home = hermes_home_env->str_val;
    len = strlen(home);
    while (len > 1 && (home[len - 1] == '/' || home[len - 1] == '\\')) len--;  /* Path() drops them */
    out = (char *)malloc(len + 40);
    if (!out) return json_null();
    memcpy(out, home, len);
    out[len] = '\0';
    strcat(out, "/workspace/meetings/nodes.json");
    {
        json_t *j = json_string(out);
        free(out);
        return j;
    }
}

