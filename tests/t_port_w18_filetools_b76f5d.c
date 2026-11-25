/* t_port_w18_filetools_b76f5d.c — harness for src/tools/file_tools_write_guards.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_filetools_b76f5d.h"

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
        const char *content = json_get_str(in, "content", "");
        json_t *out = NULL;
        if (!strcmp(func, "is_internal_file_status_text"))
            out = json_bool(w18_filetools_b76f5d_is_internal_file_status_text(content));
        else if (!strcmp(func, "looks_like_read_file_line_numbered_content"))
            out = json_bool(w18_filetools_b76f5d_looks_like_read_file_line_numbered_content(content));
        else if (!strcmp(func, "is_internal_file_tool_content"))
            out = json_bool(w18_filetools_b76f5d_is_internal_file_tool_content(content));
        else if (!strcmp(func, "stale_write_refusal")) {
            json_t *rs = json_obj_get(in, "resolved");
            const char *resolved = (rs && rs->type == JSON_STRING) ? rs->str_val : NULL;
            out = w18_filetools_b76f5d_stale_write_refusal(
                json_get_str(in, "filepath", ""), json_get_str(in, "reason", ""), resolved);
        } else
            out = json_string("UNKNOWN_FUNC");
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
