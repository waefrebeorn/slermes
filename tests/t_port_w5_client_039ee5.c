#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_client_039ee5.h"

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
        int explicit = json_get_bool(in, "explicit", 0);
        double timeout = json_get_num(in, "timeout", 1800);

        if (!strcmp(func, "prepare_tools")) {
            out = w5_client_039ee5_prepare_tools(
                json_obj_get(in, "names"),
                json_get_str(in, "outpath", ""),
                json_get_str(in, "target", ""),
                json_obj_get(in, "cache"));
        } else if (!strcmp(func, "stage_tools")) {
            out = w5_client_039ee5_stage_tools(
                json_obj_get(in, "names"),
                json_get_str(in, "source_store", ""),
                json_get_str(in, "outpath", ""),
                json_get_str(in, "target", ""));
        } else if (!strcmp(func, "ensure_environment")) {
            out = w5_client_039ee5_ensure_environment(
                json_get_str(in, "name", ""),
                json_obj_get(in, "requirements"),
                json_obj_get(in, "root"), explicit, timeout);
        } else if (!strcmp(func, "ensure_python_tool")) {
            out = w5_client_039ee5_ensure_python_tool(
                json_get_str(in, "name", ""),
                json_obj_get(in, "requirements"),
                json_get_str(in, "executable", ""),
                json_obj_get(in, "root"), explicit, timeout);
        } else if (!strcmp(func, "build_requirements_environment")) {
            out = w5_client_039ee5_build_requirements_environment(
                json_obj_get(in, "requirements"),
                json_get_str(in, "outpath", ""),
                json_obj_get(in, "python"),
                json_obj_get(in, "cache"),
                json_obj_get(in, "env"),
                json_obj_get(in, "wheelhouse"),
                json_get_bool(in, "offline", 0),
                json_get_bool(in, "sealed", 0), explicit,
                json_obj_get(in, "lock"));
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
