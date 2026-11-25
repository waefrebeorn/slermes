/* t_port_w10_skillstool_52fa6b.c -- JSON-lines harness for src/tools/skills_tool_dedup.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_skillstool_52fa6b.h"

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
        if (!strcmp(func, "dedup")) {
            int given = json_has(in, "task");
            out = w10_skillstool_52fa6b_dedup_skill_view(
                json_get_str(in, "task", ""), json_get_str(in, "name", ""),
                json_get_str(in, "file_path", ""), json_obj_get(in, "payload"),
                json_get_str(in, "result", ""));
        } else if (!strcmp(func, "reset")) {
            int given = json_has(in, "task");
            out = w10_skillstool_52fa6b_reset_skill_view_dedup(json_get_str(in, "task", ""), given);
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
