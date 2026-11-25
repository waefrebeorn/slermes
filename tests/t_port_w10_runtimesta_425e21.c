/* Port gate harness for hermes_cli/runtime_state.py (gate w10_runtimesta_425e21). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_runtimesta_425e21.h"

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
        if(!strcmp(func,"committed")) out=w10_runtimesta_425e21_committed(json_obj_get(in,"row"),json_get_str(in,"facts_digest",""));
        else if(!strcmp(func,"plugin_paths_valid")) out=w10_runtimesta_425e21_plugin_paths_valid(
            json_get_str(in,"home",""),json_get_str(in,"target",""),json_get_str(in,"backup",""),json_get_str(in,"metadata",""));
        else if(!strcmp(func,"config_path_valid")) out=w10_runtimesta_425e21_config_path_valid(json_get_str(in,"config",""),json_get_str(in,"home",""));
        else if(!strcmp(func,"path_relative_to")) out=w10_runtimesta_425e21_path_relative_to(json_get_str(in,"path",""),json_get_str(in,"parent",""));
        else if(!strcmp(func,"path_leaf")) out=w10_runtimesta_425e21_path_leaf(json_get_str(in,"path",""));
        else if(!strcmp(func,"path_parent")) out=w10_runtimesta_425e21_path_parent(json_get_str(in,"path",""));
        else out=json_string("UNKNOWN_FUNC");
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
