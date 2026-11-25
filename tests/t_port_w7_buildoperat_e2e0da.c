/* Port gate harness for pm/build_operations.py (gate w7_buildoperat_e2e0da). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_buildoperat_e2e0da.h"
#include "pm/build_operations.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int jbool(const json_t *in, const char *k) {
    return json_get_bool(in, k, false) ? 1 : 0;
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
        if (!strcmp(func, "prepare_tools")) {
            out = w7_buildoperat_prepare_tools(json_obj_get(in, "names"), jbool(in, "state_present"));
        } else if (!strcmp(func, "verified_tools")) {
            out = w7_buildoperat_verified_tools(json_obj_get(in, "names"), jbool(in, "state_present"), jbool(in, "facts_present"));
        } else if (!strcmp(func, "stage_tools")) {
            out = w7_buildoperat_stage_tools(json_obj_get(in, "names"),
                        jbool(in, "overlap"), jbool(in, "state_present"), jbool(in, "facts_present"));
        } else if (!strcmp(func, "check_project_lock")) {
            out = w7_buildoperat_check_project_lock(jbool(in, "manifest_present"),
                        jbool(in, "offline"), jbool(in, "explicit"));
        } else if (!strcmp(func, "build_env")) {
            out = w7_buildoperat_build_requirements_environment(jbool(in, "explicit"));
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
