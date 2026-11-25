#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_uv_f7190a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "toolchain")) {
            char *uv = NULL, *py = NULL, *name = NULL;
            int kind = 0;
            int code = pmuv_toolchain(
                json_get_bool(in, "realize", 1),
                json_get_str(in, "target", ""),
                json_get_bool(in, "sealed", 0),
                json_get_bool(in, "uv_loc", 0),
                json_get_bool(in, "uv_bin", 0),
                json_get_str(in, "uv_path", ""),
                json_get_bool(in, "py_loc", 0),
                json_get_bool(in, "py_bin", 0),
                json_get_str(in, "py_path", ""),
                json_get_bool(in, "writable", 0),
                &uv, &py, &kind, &name);
            if (code == PMUV_OK) {
                out = json_object();
                json_set(out, "status", json_string("ok"));
                json_set(out, "uv", json_string(uv ? uv : ""));
                json_set(out, "python", json_string(py ? py : ""));
            } else if (code == PMUV_ERROR) {
                out = json_object();
                json_set(out, "status", json_string("error"));
                json_set(out, "name", json_string(name ? name : ""));
                json_set(out, "kind", json_string(kind == PMUV_UNAVAILABLE ? "unavailable" : "missing"));
            } else {
                out = json_null();
            }
            free(uv); free(py); free(name);
        } else if (!strcmp(func, "needs_copy")) {
            out = json_bool(pmuv_needs_writable_copy(json_get_str(in, "target", ""),
                                                     json_get_bool(in, "sealed", 0)) != 0);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
