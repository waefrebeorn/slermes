/* t_port_w13_toolprogre_d54862.c — oracle harness for tui_gateway/tool_progress.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_toolprogre_d54862.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_or_null(char *s) {
    if (!s) return json_null();
    json_t *v = json_string(s); free(s); return v;
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
        if (!strcmp(func, "cap_tui_verbose_text")) {
            out = str_or_null(toolprogr_cap_tui_verbose_text(json_get_str(in, "text", "")));
        } else if (!strcmp(func, "fmt_tool_duration")) {
            out = str_or_null(toolprogr_fmt_tool_duration(json_obj_get(in, "seconds")));
        } else if (!strcmp(func, "count_list")) {
            out = toolprogr_count_list(json_obj_get(in, "obj"), json_obj_get(in, "path"));
        } else if (!strcmp(func, "tool_summary")) {
            out = toolprogr_tool_summary(json_get_str(in, "name", ""),
                                         json_get_str(in, "result", ""),
                                         json_obj_get(in, "duration_s"));
        } else if (!strcmp(func, "normalize_todo_state")) {
            out = toolprogr_normalize_todo_state(json_obj_get(in, "value"));
        } else if (!strcmp(func, "tool_result_needs_user")) {
            out = toolprogr_tool_result_needs_user(json_get_str(in, "result", ""));
        } else if (!strcmp(func, "int_or_skip")) {
            out = toolprogr_int_or_skip(json_obj_get(in, "v"));
        } else if (!strcmp(func, "str_list")) {
            out = toolprogr_str_list(json_obj_get(in, "v"));
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
