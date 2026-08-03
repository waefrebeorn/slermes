/*
 * port_skill_bundles_remaining.c — Port of agent/skill_bundles.py bundle
 * registry surface. REAL file ops: bundles dir, slugify, scanning with
 * mtime cache, save/delete/get, listing.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "json.h"
#include "yaml.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *bundle_home(void) {
    const char *h = getenv("HERMES_BUNDLES_DIR");
    if (h && *h) return h;
    h = getenv("HERMES_HOME");
    if (h && *h) return h;
    h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

/* PoP: _bundles_dir @ agent/skill_bundles.py:_bundles_dir */
char *sbd_bundles_dir(void) {
    char *out = NULL;
    const char *h = getenv("HERMES_BUNDLES_DIR");
    if (h && *h) return strdup(h);
    asprintf(&out, "%s/bundles", bundle_home());
    return out;
}

/* PoP: _slugify @ agent/skill_bundles.py:_slugify */
char *sbd_slugify(const char *name) {
    /* Python: lower, space/underscore → dash, strip invalid chars. */
    if (!name) return strdup("");
    char *out = malloc(strlen(name) * 2 + 1);
    if (!out) return NULL;
    char *q = out;
    for (const char *p = name; *p; p++) {
        char c = tolower((unsigned char)*p);
        if (c == ' ' || c == '_') *q++ = '-';
        else if (isalnum((unsigned char)c) || c == '-' || c == '.') *q++ = c;
    }
    /* collapse duplicate dashes, strip leading/trailing */
    char *dst = out;
    bool prev_dash = false;
    for (char *s = out; *s; s++) {
        if (*s == '-') {
            if (prev_dash) continue;
            if (s == out) continue;
            prev_dash = true;
        } else {
            prev_dash = false;
        }
        *dst++ = *s;
    }
    while (dst > out && dst[-1] == '-') dst--;
    *dst = '\0';
    return out;
}

/* PoP: _iter_bundle_files @ agent/skill_bundles.py:_iter_bundle_files */
char *sbd_iter_bundle_files(void) {
    /* Python: *.yaml bundle files. */
    char *dir = sbd_bundles_dir();
    DIR *d = opendir(dir);
    if (!d) { free(dir); return strdup("[]"); }
    size_t cap = 512, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); free(dir); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        bool is_yaml = n >= 5 && strcmp(e->d_name + n - 5, ".yaml") == 0;
        bool is_yml = n >= 4 && strcmp(e->d_name + n - 4, ".yml") == 0;
        if (!is_yaml && !is_yml) continue;
        size_t need = len + n + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, e->d_name);
        strcat(out, "\"");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    free(dir);
    return out;
}

/* PoP: _max_mtime @ agent/skill_bundles.py:_max_mtime */
long sbd_max_mtime(void) {
    /* Python: highest mtime across bundle files + dir. */
    char *dir = sbd_bundles_dir();
    struct stat st;
    long best = 0;
    if (stat(dir, &st) == 0) best = (long)st.st_mtime;
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            bool is_yaml = n >= 5 && strcmp(e->d_name + n - 5, ".yaml") == 0;
            bool is_yml = n >= 4 && strcmp(e->d_name + n - 4, ".yml") == 0;
            if (!is_yaml && !is_yml) continue;
            char *p = NULL;
            asprintf(&p, "%s/%s", dir, e->d_name);
            if (p && stat(p, &st) == 0 && (long)st.st_mtime > best) best = (long)st.st_mtime;
            free(p);
        }
        closedir(d);
    }
    free(dir);
    return best;
}

/* PoP: _load_bundle_file @ agent/skill_bundles.py:_load_bundle_file */
char *sbd_load_bundle_file(const char *path) {
    /* Python: parse bundle yaml; None on error. */
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    if (!buf) return NULL;
    return buf;
}

/* PoP: scan_bundles @ agent/skill_bundles.py:scan_bundles */
char *sbd_scan_bundles(void) {
    /* Python: scan + rebuild cache. Returns {"/slug": info-dict}. First
     * bundle wins on duplicate slug (alphabetical order). */
    char *files = sbd_iter_bundle_files();  /* JSON array of names */
    if (!files) return strdup("{}");
    json_t *list = json_parse(files, NULL);
    free(files);
    if (!list || list->type != JSON_ARRAY) {
        if (list) json_free(list);
        return strdup("{}");
    }

    char *dir = sbd_bundles_dir();
    json_t *out = json_object();
    for (size_t i = 0; i < json_len(list); i++) {
        json_t *name_node = json_get(list, i);
        if (!name_node || name_node->type != JSON_STRING) continue;
        const char *fname = name_node->str_val;
        if (!fname) continue;
        char *path = NULL;
        asprintf(&path, "%s/%s", dir, fname);
        if (!path) continue;

        /* Parse the bundle YAML. */
        char *raw = sbd_load_bundle_file(path);
        if (!raw) { free(path); continue; }
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse(raw, &err);
        free(raw);
        if (!doc) { free(err); free(path); continue; }

        const char *name = yaml_get_string(doc, "name");
        if (!name || !*name) name = fname; /* fall back to stem (with ext) */
        /* strip .yaml/.yml for the stem fallback */
        char stem[512];
        snprintf(stem, sizeof(stem), "%s", fname);
        char *dot = strrchr(stem, '.');
        if (dot && (strcmp(dot, ".yaml") == 0 || strcmp(dot, ".yml") == 0)) *dot = '\0';
        if (!name || !*name) name = stem;
        char *slug = sbd_slugify(name);
        if (!slug || !*slug) { free(slug); yaml_free(doc); free(path); continue; }

        /* First wins on duplicate slug. */
        char key[600];
        snprintf(key, sizeof(key), "/%s", slug);
        json_t *existing = json_obj_get(out, key);
        if (existing) { free(slug); yaml_free(doc); free(path); continue; }

        /* skills list */
        size_t nskills = yaml_list_count(doc, "skills");
        json_t *skills = json_array();
        for (size_t s = 0; s < nskills; s++) {
            const char *sk = yaml_list_get(doc, "skills", s);
            if (sk && *sk) json_append(skills, json_string(sk));
        }
        const char *description = yaml_get_string(doc, "description");
        char desc_buf[600];
        if (!description || !*description) {
            snprintf(desc_buf, sizeof(desc_buf), "Load %zu skills as a bundle", nskills);
            description = desc_buf;
        }
        const char *instruction = yaml_get_string(doc, "instruction");

        json_t *info = json_object();
        json_set(info, "name", json_string(name));
        json_set(info, "slug", json_string(slug));
        json_set(info, "description", json_string(description));
        json_set(info, "skills", skills);
        json_set(info, "instruction", json_string(instruction ? instruction : ""));
        json_set(info, "path", json_string(path));
        json_set(out, key, info);

        free(slug);
        yaml_free(doc);
        free(path);
    }
    json_free(list);
    free(dir);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{}");
}

/* Cached mapping + mtime snapshot (Python module globals). */
static char *s_bundle_cache = NULL;
static long s_bundle_cache_mtime = -1;

/* PoP: get_skill_bundles @ agent/skill_bundles.py:get_skill_bundles */
char *sbd_get_skill_bundles(void) {
    /* Python: current mapping, rescan when disk changed. */
    long now = sbd_max_mtime();
    if (!s_bundle_cache || s_bundle_cache_mtime != now) {
        char *fresh = sbd_scan_bundles();
        if (fresh) {
            free(s_bundle_cache);
            s_bundle_cache = fresh;
            s_bundle_cache_mtime = now;
        }
    }
    return s_bundle_cache ? strdup(s_bundle_cache) : strdup("{}");
}

/* PoP: reload_bundles @ agent/skill_bundles.py:reload_bundles */
char *sbd_reload_bundles(void) {
    /* Python: re-scan + diff {added, removed, unchanged, total}. */
    char *before = s_bundle_cache ? strdup(s_bundle_cache) : NULL;
    char *new = sbd_scan_bundles();
    if (!new) { free(before); return strdup("{\"added\":[],\"removed\":[],\"unchanged\":[],\"total\":0}"); }

    /* Snapshot: slug → description. */
    json_t *before_j = before ? json_parse(before, NULL) : json_object();
    json_t *after_j = json_parse(new, NULL);
    free(before);
    free(new);
    if (!after_j) {
        if (before_j) json_free(before_j);
        return strdup("{\"added\":[],\"removed\":[],\"unchanged\":[],\"total\":0}");
    }
    if (!before_j) before_j = json_object();

    json_t *added = json_array(), *removed = json_array();
    json_t *unchanged = json_array();
    /* Iterate object entries via the libjson struct (keys/items/count). */
    size_t a_count = after_j->type == JSON_OBJECT ? after_j->c.count : 0;
    size_t b_count = before_j->type == JSON_OBJECT ? before_j->c.count : 0;
    for (size_t i = 0; i < a_count; i++) {
        const char *key = after_j->c.keys[i];
        bool in_before = false;
        for (size_t j = 0; j < b_count; j++)
            if (strcmp(key, before_j->c.keys[j]) == 0) { in_before = true; break; }
        if (!in_before) {
            json_t *info = json_obj_get(after_j, key);
            json_append(added, info ? json_copy(info) : json_string(key));
        } else {
            json_t *info_b = json_obj_get(before_j, key);
            json_t *info_a = json_obj_get(after_j, key);
            if (info_b && info_a &&
                strcmp(json_get_str(info_b, "description", ""),
                       json_get_str(info_a, "description", "")) == 0)
                json_append(unchanged, json_string(key));
            else
                json_append(added, info_a ? json_copy(info_a) : json_string(key));
        }
    }
    for (size_t j = 0; j < b_count; j++) {
        const char *key = before_j->c.keys[j];
        bool in_after = false;
        for (size_t i = 0; i < a_count; i++)
            if (strcmp(key, after_j->c.keys[i]) == 0) { in_after = true; break; }
        if (!in_after) {
            json_t *info = json_obj_get(before_j, key);
            json_append(removed, info ? json_copy(info) : json_string(key));
        }
    }

    json_t *out = json_object();
    json_set(out, "added", added);
    json_set(out, "removed", removed);
    json_set(out, "unchanged", unchanged);
    json_set(out, "total", json_number((double)a_count));

    /* Update the cache to the fresh scan (before freeing after_j). */
    free(s_bundle_cache);
    s_bundle_cache = json_serialize(after_j);
    s_bundle_cache_mtime = sbd_max_mtime();

    json_free(before_j);
    json_free(after_j);

    char *ser = json_serialize(out);
    json_free(out);
    return ser ? ser : strdup("{\"added\":[],\"removed\":[],\"unchanged\":[],\"total\":0}");
}

/* PoP: list_bundles @ agent/skill_bundles.py:list_bundles */
char *sbd_list_bundles(void) {
    /* Python: sorted bundle info dicts by slug. */
    char *map = sbd_get_skill_bundles();
    if (!map) return strdup("[]");
    json_t *obj = json_parse(map, NULL);
    free(map);
    if (!obj) return strdup("[]");

    size_t count = obj->type == JSON_OBJECT ? obj->c.count : 0;
    json_t *arr = json_array();
    /* Insertion sort by slug while appending. */
    for (size_t i = 0; i < count; i++) {
        json_t *info = json_obj_get(obj, obj->c.keys[i]);
        if (!info) continue;
        const char *slug = json_get_str(info, "slug", obj->c.keys[i]);
        size_t pos = json_len(arr);
        for (size_t j = 0; j < json_len(arr); j++) {
            json_t *other = json_get(arr, j);
            const char *other_slug = other ? json_get_str(other, "slug", "") : "";
            if (strcmp(slug, other_slug) < 0) { pos = j; break; }
        }
        json_t *copy = json_copy(info);
        if (pos == json_len(arr)) json_append(arr, copy);
        else {
            /* insert at pos by rebuilding */
            json_t *tmp = json_array();
            for (size_t j = 0; j < json_len(arr); j++) {
                if (j == pos) json_append(tmp, copy);
                json_append(tmp, json_copy(json_get(arr, j)));
            }
            json_free(arr);
            arr = tmp;
        }
    }
    json_free(obj);

    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: bundle_path_for @ agent/skill_bundles.py:bundle_path_for */
char *sbd_bundle_path_for(const char *name) {
    /* Python: canonical path for bundle name. */
    if (!name) return NULL;
    char *slug = sbd_slugify(name);
    char *dir = sbd_bundles_dir();
    char *out = NULL;
    asprintf(&out, "%s/%s.yaml", dir, slug);
    free(slug);
    free(dir);
    return out;
}

/* PoP: save_bundle @ agent/skill_bundles.py:save_bundle */
int sbd_save_bundle(const char *name, const char *yaml_text) {
    /* Python: write + invalidate cache; FileExistsError when present. */
    if (!name || !yaml_text) return -1;
    char *path = sbd_bundle_path_for(name);
    if (access(path, F_OK) == 0) { free(path); return 1; }  /* exists */
    mkdir(sbd_bundles_dir(), 0755);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fwrite(yaml_text, 1, strlen(yaml_text), w);
    fclose(w);
    free(path);
    return 0;
}

/* PoP: delete_bundle @ agent/skill_bundles.py:delete_bundle */
char *sbd_delete_bundle(const char *name) {
    /* Python: delete by name; FileNotFoundError when missing. */
    if (!name) return NULL;
    char *path = sbd_bundle_path_for(name);
    if (access(path, F_OK) != 0) { free(path); return NULL; }
    int rc = unlink(path);
    char *out = NULL;
    if (rc == 0) asprintf(&out, "%s", path);
    free(path);
    return out;
}

/* PoP: get_bundle @ agent/skill_bundles.py:get_bundle */
char *sbd_get_bundle(const char *name) {
    /* Python: lookup by slug-normalized name. */
    if (!name) return NULL;
    char *path = sbd_bundle_path_for(name);
    char *out = sbd_load_bundle_file(path);
    free(path);
    return out;
}
