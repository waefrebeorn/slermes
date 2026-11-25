#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_cliinfomi_ea6329.h"

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
        if (!strcmp(func, "tool_progress_label")) {
            out = w12_cliinfomi_ea6329_tool_progress_label(json_get_str(in, "mode", ""));
        } else if (!strcmp(func, "reload_mcp_choices")) {
            out = w12_cliinfomi_ea6329_reload_mcp_choices();
        } else if (!strcmp(func, "help_section_title")) {
            out = w12_cliinfomi_ea6329_help_section_title(json_get_str(in, "category", ""));
        } else if (!strcmp(func, "skill_line")) {
            out = w12_cliinfomi_ea6329_skill_line(json_obj_get(in, "item"));
        } else if (!strcmp(func, "toolset_map")) {
            out = w12_cliinfomi_ea6329_toolset_map(json_obj_get(in, "tools"),
                        json_obj_get(in, "availability"), json_obj_get(in, "toolset_of"));
        } else if (!strcmp(func, "ascii_box")) {
            out = w12_cliinfomi_ea6329_ascii_box(json_get_str(in, "title", ""),
                        (int)json_get_num(in, "width", 0));
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
