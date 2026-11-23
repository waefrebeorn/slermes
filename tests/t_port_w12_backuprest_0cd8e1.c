#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_backuprest_0cd8e1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "validate")) {
            out = w12_backuprest_0cd8e1_validate_backup_zip(json_obj_get(in, "names"));
        } else if (!strcmp(func, "prefix")) {
            out = w12_backuprest_0cd8e1_detect_prefix(json_obj_get(in, "names"));
        } else if (!strcmp(func, "get")) {
            out = w12_backuprest_0cd8e1_get_config_path_value(json_obj_get(in, "data"),
                                                              json_obj_get(in, "dotted"));
        } else if (!strcmp(func, "set")) {
            out = w12_backuprest_0cd8e1_set_config_path_value(json_obj_get(in, "data"),
                        json_obj_get(in, "dotted"), json_obj_get(in, "value"));
        } else if (!strcmp(func, "is_store")) {
            out = w12_backuprest_0cd8e1_is_store_file(json_get_str(in, "name", ""),
                                                      json_get_str(in, "resolved", ""));
        } else if (!strcmp(func, "names")) {
            out = w12_backuprest_0cd8e1_store_file_names();
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
