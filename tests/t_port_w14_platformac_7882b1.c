/* t_port_w14_platformac_7882b1.c — oracle harness for
 * gateway/platform_activation.py (gate w14_platformac_7882b1). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_platformac_7882b1.h"

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

        if (!strcmp(func, "yaml_block_names")) {
            out = w14_platformac_7882b1_yaml_block_names(json_obj_get(in, "yaml_cfg"));
        } else if (!strcmp(func, "matches")) {
            out = w14_platformac_7882b1_matches(json_get_str(in, "name", ""),
                                                json_obj_get(in, "candidates"));
        } else if (!strcmp(func, "has_evidence")) {
            /* activation_predicate assembles blocks = yaml blocks | platform
             * names | legacy gateway.json names before has_evidence runs. */
            json_t *blocks = json_array();
            const char *srcs[] = {"blocks", "platform_names", "legacy", NULL};
            for (size_t s = 0; srcs[s]; ++s) {
                json_t *a = json_obj_get(in, srcs[s]);
                if (a && a->type == JSON_ARRAY)
                    for (size_t i = 0; i < json_len(a); ++i)
                        json_append(blocks, json_copy(json_get(a, i)));
            }
            out = w14_platformac_7882b1_has_evidence(
                json_get_str(in, "name", ""), blocks,
                json_obj_get(in, "pools"), json_obj_get(in, "env"),
                json_obj_get(in, "declared"));
            json_free(blocks);
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
