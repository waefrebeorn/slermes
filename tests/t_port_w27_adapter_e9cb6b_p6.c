/* JSON-lines harness for the w27_adapter_e9cb6b_p6 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_e9cb6b_p6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w27_adapter_constants();
        } else if (!strcmp(func, "model_picker_reactions")) {
            out = w27_adapter_model_picker_reactions();
        } else if (!strcmp(func, "choice_picker_reactions")) {
            out = w27_adapter_choice_picker_reactions();
        } else if (!strcmp(func, "approval_reaction_map")) {
            out = w27_adapter_approval_reaction_map();
        } else if (!strcmp(func, "yaml_bridge")) {
            out = w27_adapter_yaml_bridge();
        } else if (!strcmp(func, "image_exts")) {
            out = w27_adapter_image_exts();
        } else if (!strcmp(func, "media_exts")) {
            out = w27_adapter_media_exts();
        } else if (!strcmp(func, "txn_id")) {
            out = w27_adapter_txn_id(json_obj_get(in, "ms"), json_obj_get(in, "rand_hex"));
        } else if (!strcmp(func, "duplicate_events")) {
            out = w27_adapter_duplicate_events(json_obj_get(in, "ids"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
