/* JSON-lines harness for the w23_imagegener_6a7ba9 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_imagegener_6a7ba9.h"

static json_t *opt_string(const char *s) {
    return s ? json_string(s) : json_null();
}

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "backend_for_model")) {
            out = json_string(w23_imagegener_6a7ba9_backend_for_model(json_obj_get(in, "model_id")));
        } else if (!strcmp(func, "route")) {
            out = opt_string(w23_imagegener_6a7ba9_route(json_obj_get(in, "provider"),
                                                         json_obj_get(in, "model_id")));
        } else if (!strcmp(func, "portal_duplicate_of")) {
            out = opt_string(w23_imagegener_6a7ba9_portal_duplicate_of(json_obj_get(in, "model_id")));
        } else if (!strcmp(func, "is_fal_krea_entry")) {
            out = json_bool(w23_imagegener_6a7ba9_is_fal_krea_entry(json_obj_get(in, "model_id")) != 0);
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
