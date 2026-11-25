/* JSON-lines harness for the w22_pluginvali_a951b0 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_pluginvali_a951b0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

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
        if (!strcmp(func, "label")) {
            char *label = w22_pluginvali_a951b0_locale_label(json_get_str(in, "lang_id", ""),
                                                             json_get_str(in, "surface", ""));
            out = json_string(label ? label : "");
            free(label);
        } else if (!strcmp(func, "flatten")) {
            out = w22_pluginvali_a951b0_flatten_keys(json_obj_get(in, "document"));
        } else if (!strcmp(func, "bad_leaves")) {
            out = w22_pluginvali_a951b0_non_text_leaves(json_obj_get(in, "document"));
        } else if (!strcmp(func, "known_ids")) {
            out = w22_pluginvali_a951b0_known_ids(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "check")) {
            out = w22_pluginvali_a951b0_check_document(json_obj_get(in, "document"),
                                                       json_get_str(in, "lang_id", ""),
                                                       json_get_str(in, "surface", "core"),
                                                       json_obj_get(in, "reference"),
                                                       json_get_str(in, "filename", "x.yaml"),
                                                       json_get_str(in, "parse_error", NULL));
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
