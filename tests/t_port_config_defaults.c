/*
 * t_port_config_defaults.c — parity harness for src/hermes_cli/config_defaults.c
 * (hermes_cli/config_defaults.py pure-data tables).
 *
 * Reads a JSON fixture (argv[1]) with "ops": list of {kind, path?, ...} and
 * emits one JSON line per op, mirroring tests/sta_oracle_config_defaults.py.
 */
#include "hermes_json.h"
#include "config_defaults.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

/* Navigate a dotted path ("a.b.0.c"); numeric segments are array indices.
 * Returns NULL when any hop is missing. */
static const json_t *nav(const json_t *root, const char *path) {
    if (!path || !*path) return root;
    const json_t *cur = root;
    char seg[256];
    const char *p = path;
    while (*p) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len >= sizeof(seg)) return NULL;
        memcpy(seg, p, len); seg[len] = '\0';
        int numeric = 1;
        for (size_t i = 0; i < len; i++)
            if (!isdigit((unsigned char)seg[i])) { numeric = 0; break; }
        if (!cur) return NULL;
        if (numeric && cur->type == JSON_ARRAY) {
            cur = json_get((json_t *)cur, (int)atoi(seg));
        } else if (cur->type == JSON_OBJECT) {
            cur = json_object_get((json_t *)cur, seg);
        } else {
            return NULL;
        }
        if (!cur) return NULL;
        p = dot ? dot + 1 : p + len;
    }
    return cur;
}

static const char *type_name(const json_t *n) {
    if (!n) return "missing";
    switch (n->type) {
        case JSON_OBJECT: return "object";
        case JSON_ARRAY:  return "array";
        case JSON_STRING: return "string";
        case JSON_NUMBER: return "number";
        case JSON_BOOL:   return n->bool_val ? "true" : "false";
        case JSON_NULL:   return "null";
        default:          return "unknown";
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "full_default") == 0) {
                json_object_set(o, "out",
                    json_string(config_defaults_DEFAULT_CONFIG_json()));
            } else if (strcmp(kind, "full_env") == 0) {
                json_object_set(o, "out",
                    json_string(config_defaults_OPTIONAL_ENV_VARS_json()));
            } else if (strcmp(kind, "keys_at") == 0) {
                json_t *t = (json_t *)nav(
                    config_defaults_DEFAULT_CONFIG(),
                    json_get_str(c, "path", ""));
                json_t *arr = json_new_array();
                if (t && t->type == JSON_OBJECT) {
                    /* iterate object keys in insertion order */
                    for (size_t k = 0; k < json_object_size(t); k++) {
                        const char *key = json_object_get_key_at(t, k);
                        if (key) json_array_append(arr, json_string(key));
                    }
                }
                json_object_set(o, "out", arr);
            } else if (strcmp(kind, "keys_env_at") == 0) {
                json_t *t = (json_t *)nav(
                    config_defaults_OPTIONAL_ENV_VARS(),
                    json_get_str(c, "path", ""));
                json_t *arr = json_new_array();
                if (t && t->type == JSON_OBJECT) {
                    for (size_t k = 0; k < json_object_size(t); k++) {
                        const char *key = json_object_get_key_at(t, k);
                        if (key) json_array_append(arr, json_string(key));
                    }
                }
                json_object_set(o, "out", arr);
            } else if (strcmp(kind, "type_at") == 0) {
                json_t *dc = config_defaults_DEFAULT_CONFIG();
                const json_t *t = nav(dc, json_get_str(c, "path", ""));
                json_object_set(o, "out", json_string(type_name(t)));
                json_free(dc);
            } else if (strcmp(kind, "str_at") == 0) {
                json_t *dc = config_defaults_DEFAULT_CONFIG();
                const json_t *t = nav(dc, json_get_str(c, "path", ""));
                if (t && t->type == JSON_STRING)
                    json_object_set(o, "out", json_string(t->str_val));
                else
                    json_object_set(o, "out", json_new_null());
                json_free(dc);
            } else if (strcmp(kind, "count_default_keys") == 0) {
                json_t *dc = config_defaults_DEFAULT_CONFIG();
                json_object_set(o, "out",
                    json_int((long long)json_object_size(dc)));
                json_free(dc);
            } else if (strcmp(kind, "count_env_vars") == 0) {
                json_t *ev = config_defaults_OPTIONAL_ENV_VARS();
                json_object_set(o, "out",
                    json_int((long long)json_object_size(ev)));
                json_free(ev);
            } else if (strcmp(kind, "fresh_parse_check") == 0) {
                /* two parses give independent trees with equal content */
                json_t *a = config_defaults_DEFAULT_CONFIG();
                json_t *b = config_defaults_DEFAULT_CONFIG();
                json_object_set(a, "__probe__", json_int(1));
                int independent = json_object_get(b, "__probe__") == NULL;
                char *sa = json_serialize(a), *sb = json_serialize(b);
                json_object_del(a, "__probe__");
                char *sa2 = json_serialize(a);
                json_object_set(o, "independent", json_bool(independent));
                json_object_set(o, "restored", json_bool(sa2 && sb && strcmp(sa2, sb) == 0));
                free(sa); free(sb); free(sa2);
                json_free(a); json_free(b);
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
