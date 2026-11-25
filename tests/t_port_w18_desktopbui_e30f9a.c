#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_desktopbui_e30f9a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
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
        if (!strcmp(func, "stamp_drift")) {
            const json_t *bv = json_obj_get(in, "binary");
            const char *binary = (bv && bv->type == JSON_STRING && bv->str_val) ? bv->str_val : NULL;
            out = w18_desktopbui_e30f9a_stamp_drift(binary, json_get_str(in, "stdout", ""),
                                                    jbool(in, "fail") ? 0 : 1);
        } else if (!strcmp(func, "packaged_desktop_reusable")) {
            out = w18_desktopbui_e30f9a_packaged_desktop_reusable(
                jbool(in, "resources_present"), json_obj_get(in, "baked"),
                json_obj_get(in, "prepared"), jbool(in, "build_needed"),
                json_obj_get(in, "drift"));
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
