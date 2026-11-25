/* JSON-lines harness for the w21_registry_5bbc35 gate
 * (plugins/google_meet/node/registry.py).
 *
 * Every case runs against an isolated temp home (SLERMES_HOME/HOME are set by
 * the gate runner); a relative "path" is resolved under HOME so nothing here
 * can touch the real registry. `preload` seeds the file with raw bytes.
 *
 * out = {"result": <op result>, "file": <parsed registry file, or null>}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "noc_w21_registry_5bbc35.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *home_dir(void) {
    const char *home = getenv("SLERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    return home ? home : "";
}

/* Absolute paths pass through; relative ones live under the temp home. */
static char *resolve_path(const char *rel) {
    const char *home = home_dir();
    size_t need;
    char *out;
    if (!rel) return NULL;
    if (rel[0] == '/' || rel[0] == '\\' || (rel[0] && rel[1] == ':')) {
        out = (char *)malloc(strlen(rel) + 1);
        if (out) strcpy(out, rel);
        return out;
    }
    need = strlen(home) + strlen(rel) + 2;
    out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s/%s", home, rel);
    return out;
}

static void make_parents(const char *path) {
    char *copy;
    char *p;
    if (!path) return;
    copy = (char *)malloc(strlen(path) + 1);
    if (!copy) return;
    strcpy(copy, path);
    for (p = copy + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mkdir(copy, 0755);
            *p = sep;
        }
    }
    free(copy);
}

static void seed_file(const char *path, const json_t *preload) {
    FILE *fh;
    if (!path || !preload || preload->type != JSON_STRING) return;
    make_parents(path);
    fh = fopen(path, "wb");
    if (!fh) return;
    fputs(preload->str_val ? preload->str_val : "", fh);
    fclose(fh);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL, *result = NULL;
        json_t *file_view = NULL;
        const char *func, *path;
        char *resolved = NULL;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        path = json_get_str(in, "path", "");
        if (strcmp(func, "default_path") != 0) resolved = resolve_path(path);
        seed_file(resolved, json_obj_get(in, "preload"));

        if (!strcmp(func, "default_path")) {
            result = w21_registry_5bbc35_default_path(json_obj_get(in, "hermes_home"));
        } else if (!strcmp(func, "get")) {
            result = w21_registry_5bbc35_get(resolved, json_get_str(in, "name", ""));
        } else if (!strcmp(func, "add")) {
            json_t *now = json_obj_get(in, "now");
            result = w21_registry_5bbc35_add(resolved, json_obj_get(in, "name"),
                                             json_obj_get(in, "url"), json_obj_get(in, "token"),
                                             now ? now->num_val : 0.0);
        } else if (!strcmp(func, "remove")) {
            result = w21_registry_5bbc35_remove(resolved, json_get_str(in, "name", ""));
        } else if (!strcmp(func, "list")) {
            result = w21_registry_5bbc35_list_all(resolved);
        } else if (!strcmp(func, "resolve")) {
            result = w21_registry_5bbc35_resolve(resolved, json_obj_get(in, "chrome_node"));
        } else {
            result = json_string("UNKNOWN_FUNC");
        }

        if (strcmp(func, "default_path") != 0) {
            /* the on-disk document as the next reader would parse it */
            file_view = w21_registry_5bbc35_read_document(resolved);
        }

        out = json_object();
        json_set(out, "result", result ? result : json_null());
        if (strcmp(func, "default_path") != 0) {
            json_set(out, "file", file_view ? file_view : json_null());
        } else {
            json_set(out, "file", json_null());
        }

        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
        free(resolved);
    }
    return 0;
}
