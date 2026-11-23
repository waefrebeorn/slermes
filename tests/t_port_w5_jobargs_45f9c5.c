#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_jobargs_45f9c5.h"

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
        if (!strcmp(func, "split_monitor_arg")) {
            out = w5_jobargs_45f9c5_split_monitor_arg(json_obj_get(in, "monitor"),
                        json_obj_get(in, "monitor_script"), json_obj_get(in, "monitor_url"));
        } else if (!strcmp(func, "repeat_display")) {
            out = w5_jobargs_45f9c5_repeat_display(json_obj_get(in, "job"));
        } else if (!strcmp(func, "clean_str_list")) {
            out = w5_jobargs_45f9c5_clean_str_list(json_obj_get(in, "items"));
        } else if (!strcmp(func, "canonical_skills")) {
            out = w5_jobargs_45f9c5_canonical_skills(json_obj_get(in, "skill"),
                                                     json_obj_get(in, "skills"));
        } else if (!strcmp(func, "normalize_optional")) {
            out = w5_jobargs_45f9c5_normalize_optional(json_obj_get(in, "value"),
                        json_get_bool(in, "strip_trailing_slash", false) ? 1 : 0);
        } else if (!strcmp(func, "normalize_deliver")) {
            out = w5_jobargs_45f9c5_normalize_deliver(json_obj_get(in, "value"));
        } else if (!strcmp(func, "apply_continuity")) {
            out = w5_jobargs_45f9c5_apply_continuity(json_obj_get(in, "context_from"),
                        json_get_bool(in, "continuity", false) ? 1 : 0);
        } else if (!strcmp(func, "base_url_refused")) {
            out = w5_jobargs_45f9c5_base_url_refused(json_get_str(in, "bu", ""),
                        json_get_str(in, "prov", ""), json_get_str(in, "why", ""));
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
