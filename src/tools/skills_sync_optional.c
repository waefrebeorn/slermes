/*
 * skills_sync_optional.c — C port of upstream Hermes tools/skills_sync_optional.py.
 *
 * PoP: w14_skillssync_38160e_safe_rel_install_path @ tools/skills_sync_optional.py:_safe_rel_install_path
 * PoP: w14_skillssync_38160e_hub_lock_entries @ tools/skills_sync_optional.py:_hub_lock_entries
 * PoP: w14_skillssync_38160e_read_hub_install_paths @ tools/skills_sync_optional.py:_read_hub_install_paths
 * PoP: w14_skillssync_38160e_is_runtime_cache @ tools/skills_sync_optional.py:_is_runtime_cache
 *
 * Filesystem-dependent predicates the Python functions ask of pathlib
 * (is_dir / with_suffix(".py").is_file) are injected as fixture flags so the
 * same decision tree can be exercised deterministically in C.
 */
#include "noc_w14_skillssync_38160e.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct json_t *value_error(void) {
    struct json_t *e = json_object();
    json_set(e, "error", json_string("ValueError"));
    return e;
}

struct json_t *w14_skillssync_38160e_safe_rel_install_path(const char *path,
                                                           const char *base) {
    char parts_buf[512][512];
    size_t nparts = 0;
    const char *rel, *p, *seg;
    char out[4096];
    size_t olen = 0;

    if (!path || !base) return value_error();
    size_t blen = strlen(base);
    if (strncmp(path, base, blen) != 0) return value_error();
    rel = path + blen;
    if (*rel && *rel != '/') return value_error();  /* not a path-component boundary */
    if (*rel == '/') rel++;
    if (!*rel) return value_error();                /* Path(".") has no parts */

    p = rel;
    while (*p) {
        const char *end = strchr(p, '/');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.')) {
            /* filter "" and "." exactly like the Python list comprehension */
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            return value_error();
        } else {
            if (nparts >= 512 || len >= 512) return value_error();
            memcpy(parts_buf[nparts], p, len);
            parts_buf[nparts][len] = '\0';
            nparts++;
        }
        if (!end) break;
        p = end + 1;
    }
    if (!nparts) return value_error();

    for (size_t i = 0; i < nparts; i++) {
        size_t len = strlen(parts_buf[i]);
        if (i) { out[olen++] = '/'; }
        if (olen + len >= sizeof out) return value_error();
        memcpy(out + olen, parts_buf[i], len);
        olen += len;
    }
    out[olen] = '\0';
    return json_string(out);
}

struct json_t *w14_skillssync_38160e_hub_lock_entries(const struct json_t *data) {
    struct json_t *out = json_array();
    const struct json_t *installed;

    if (!json_is_object(data)) return out;
    installed = json_obj_get(data, "installed");
    if (!json_is_object(installed)) return out;
    for (size_t i = 0; i < json_object_size(installed); i++) {
        struct json_t *v = json_object_get_at(installed, i);
        if (json_is_object(v)) json_append(out, json_copy(v));
    }
    return out;
}

/* Python str() for the scalar JSON types used by hub lock metadata. */
static void py_str(const struct json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
        case JSON_STRING:
            snprintf(buf, n, "%s", v->str_val);
            break;
        case JSON_BOOL:
            snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
            break;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d >= -9007199254740992.0 && d <= 9007199254740992.0)
                snprintf(buf, n, "%lld", (long long)d);
            else
                snprintf(buf, n, "%g", d);
            break;
        }
        default: {
            char *s = json_dumps(v, 0);
            snprintf(buf, n, "%s", s ? s : "");
            free(s);
            break;
        }
    }
}

static int py_truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

struct json_t *w14_skillssync_38160e_read_hub_install_paths(const struct json_t *data) {
    struct json_t *entries = w14_skillssync_38160e_hub_lock_entries(data);
    struct json_t *out = json_array();
    char paths[256][1024];
    size_t count = 0;

    for (size_t i = 0; i < json_len(entries); i++) {
        const struct json_t *e = json_get(entries, i);
        const struct json_t *ip = json_obj_get(e, "install_path");
        char raw[1024], *a, *b, tmp[1024];
        size_t len;
        if (!py_truthy(ip)) continue;
        py_str(ip, raw, sizeof raw);
        a = raw;
        b = raw + strlen(raw);
        while (a < b && *a == '/') a++;
        while (b > a && b[-1] == '/') b--;
        len = (size_t)(b - a);
        if (count >= 256 || len >= sizeof tmp) continue;
        memcpy(tmp, a, len);
        tmp[len] = '\0';
        /* insertion sort keeps output deterministic (Python set -> sorted) */
        size_t j = count;
        while (j > 0 && strcmp(paths[j - 1], tmp) > 0) {
            memcpy(paths[j], paths[j - 1], sizeof paths[0]);
            j--;
        }
        if (j > 0 && strcmp(paths[j - 1], tmp) == 0) continue;  /* Python set() */
        snprintf(paths[j], sizeof paths[j], "%s", tmp);
        count++;
    }
    for (size_t i = 0; i < count; i++) json_append(out, json_string(paths[i]));
    json_free(entries);
    return out;
}

static int is_cache_dir(const char *s) {
    return !strcmp(s, "__pycache__") || !strcmp(s, ".pytest_cache") ||
           !strcmp(s, ".mypy_cache") || !strcmp(s, ".ruff_cache");
}

int w14_skillssync_38160e_is_runtime_cache(const char *path, const char *skill_dir,
                                           int is_dir, int py_exists) {
    char parts[128][512];
    size_t n = 0;
    const char *rel, *p;
    size_t blen;
    const char *name;
    const char *dot;

    if (!path || !skill_dir) return 0;
    blen = strlen(skill_dir);
    if (strncmp(path, skill_dir, blen) != 0) return 0;
    rel = path + blen;
    if (*rel && *rel != '/') return 0;
    if (*rel == '/') rel++;

    p = rel;
    while (*p) {
        const char *end = strchr(p, '/');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len && len < 512 && n < 128) {
            memcpy(parts[n], p, len);
            parts[n][len] = '\0';
            n++;
        }
        if (!end) break;
        p = end + 1;
    }
    if (!n) return 0;

    /* any(part in _RUNTIME_CACHE_DIRS for part in relative.parts[:-1]) */
    if (n > 1)
        for (size_t i = 0; i + 1 < n; i++)
            if (is_cache_dir(parts[i])) return 1;

    name = parts[n - 1];
    if (is_cache_dir(name) && is_dir) return 1;

    dot = strrchr(name, '.');
    if (dot && dot > name && dot[1]) {
        if (!strcmp(dot, ".pyc") || !strcmp(dot, ".pyo"))
            return py_exists ? 1 : 0;
    }
    return 0;
}
