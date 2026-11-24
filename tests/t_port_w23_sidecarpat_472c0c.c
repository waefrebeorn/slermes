/* JSON-lines harness for the w23_sidecarpat_472c0c gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sidecarpat_472c0c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
}

static json_t *string_out(char *take) {
    json_t *o = json_string(take ? take : "");
    free(take);
    return o;
}

static json_t *mirror_files_out(void) {
    json_t *arr = json_array();
    size_t i, n = w23_sidecarpat_472c0c_mirror_file_count();
    for (i = 0; i < n; i++)
        json_append(arr, json_string(w23_sidecarpat_472c0c_mirror_file(i)));
    return arr;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out, *world;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        world = json_obj_get(in, "world");
        if (!strcmp(func, "resolve")) {
            out = w23_sidecarpat_472c0c_resolve(world);
        } else if (!strcmp(func, "sidecar_dir")) {
            out = string_out(w23_sidecarpat_472c0c_sidecar_dir(world));
        } else if (!strcmp(func, "npm_error_log")) {
            out = string_out(w23_sidecarpat_472c0c_npm_error_log(world));
        } else if (!strcmp(func, "writable")) {
            out = json_bool(w23_sidecarpat_472c0c_dir_writable(world) ? 1 : 0);
        } else if (!strcmp(func, "mirror_files")) {
            out = mirror_files_out();
        } else if (!strcmp(func, "source_default")) {
            out = string_out(w23_sidecarpat_472c0c_source_default());
        } else if (!strcmp(func, "path_norm")) {
            const char *p = json_get_str(world, "path", "");
            out = string_out(w23_sidecarpat_472c0c_path_norm(p));
        } else {
            out = json_string("UNKNOWN_FUNC");
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
        json_free(in);
    }
    return 0;
}
