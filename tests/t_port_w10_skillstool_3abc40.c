/* t_port_w10_skillstool_3abc40.c -- JSON-lines harness for src/tools/skills_tool_setup.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_skillstool_3abc40.h"

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
        if (!strcmp(func, "remote")) {
            out = w10_skillstool_3abc40_is_remote_env_backend(json_get_str(in, "backend", ""));
        } else if (!strcmp(func, "aslist")) {
            out = w10_skillstool_3abc40_as_dict_list(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "clean")) {
            out = w10_skillstool_3abc40_clean_str(json_obj_get(in, "value"));
        } else if (!strcmp(func, "required")) {
            out = w10_skillstool_3abc40_get_required_environment_variables(json_obj_get(in, "fm"));
        } else if (!strcmp(func, "capture")) {
            out = w10_skillstool_3abc40_capture_result(json_obj_get(in, "missing"),
                    json_get_bool(in, "skipped", false) ? 1 : 0, json_obj_get(in, "hint"));
        } else if (!strcmp(func, "note")) {
            out = w10_skillstool_3abc40_build_setup_note(json_get_str(in, "status", ""),
                    json_obj_get(in, "missing"), json_obj_get(in, "help"));
        } else if (!strcmp(func, "persisted")) {
            out = w10_skillstool_3abc40_is_env_var_persisted(json_get_str(in, "var", ""),
                    json_obj_get(in, "snapshot"), json_obj_get(in, "secret"));
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
