/* JSON-lines harness for the w21_todo_138ed8 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_todo_138ed8.h"
#include "persistence/sqlite/conversation/_store/todo.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *tri(int v) { return json_number((double)v); }

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_call")) {
            out = tri(w21_todo_138ed8_is_todo_call(json_obj_get(in, "call"),
                                                   json_get_str(in, "call_id", "")));
        } else if (!strcmp(func, "paired")) {
            out = tri(w21_todo_138ed8_paired(json_get_str(in, "tool_calls", NULL),
                                             json_get_str(in, "call_id", "")));
        } else if (!strcmp(func, "valid")) {
            out = tri(w21_todo_138ed8_valid_result(json_get_str(in, "content", NULL)));
        } else if (!strcmp(func, "copy_ids")) {
            out = w21_todo_138ed8_copy_ids(json_obj_get(in, "identity"),
                                           json_get_str(in, "message_uid", NULL),
                                           json_obj_get(in, "absorbed"));
        } else if (!strcmp(func, "removed")) {
            out = w21_todo_138ed8_removed_map(json_obj_get(in, "rows"));
        } else if (!strcmp(func, "removed_hit")) {
            out = tri(w21_todo_138ed8_copy_removed(json_obj_get(in, "removed"),
                                                   json_obj_get(in, "copy_ids"),
                                                   json_obj_get(in, "row_id")));
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
