/* JSON-lines harness for the w22_availabili_9d8d88 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_availabili_9d8d88.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *tri(int v) { return json_number((double)v); }

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "vparts")) {
            out = w22_availabili_9d8d88_version_parts(json_get_str(in, "version", NULL));
        } else if (!strcmp(func, "at_least")) {
            out = tri(w22_availabili_9d8d88_version_at_least(json_get_str(in, "found", NULL),
                                                            json_get_str(in, "minimum", NULL)));
        } else if (!strcmp(func, "offerable")) {
            out = tri(w22_availabili_9d8d88_offerable(json_get_str(in, "state", NULL)));
        } else if (!strcmp(func, "as_dict")) {
            out = w22_availabili_9d8d88_as_dict(json_get_str(in, "state", ""),
                                               json_get_str(in, "version", NULL),
                                               json_get_str(in, "path", NULL),
                                               json_get_str(in, "minimum", NULL));
        } else if (!strcmp(func, "evaluate")) {
            out = w22_availabili_9d8d88_evaluate(json_obj_get(in, "entry"),
                                                 json_get_str(in, "os_family", NULL),
                                                 json_get_str(in, "gpu_class", NULL),
                                                 json_obj_get(in, "app"));
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
