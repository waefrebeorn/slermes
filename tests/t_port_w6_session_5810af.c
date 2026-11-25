/* Port gate harness for acp_adapter/session.py (gate w6_session_5810af). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_session_5810af.h"

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
        if (!strcmp(func, "build_session_title")) {
            out = acpsess_build_session_title(json_obj_get(in, "title"),
                                              json_obj_get(in, "preview"),
                                              json_get_str(in, "cwd", ""));
        } else if (!strcmp(func, "format_updated_at")) {
            out = acpsess_format_updated_at(json_obj_get(in, "value"));
        } else if (!strcmp(func, "updated_at_sort_key")) {
            out = acpsess_updated_at_sort_key(json_obj_get(in, "value"));
        } else if (!strcmp(func, "parse_model_config")) {
            out = acpsess_parse_model_config(json_obj_get(in, "mc"));
        } else if (!strcmp(func, "expand_toolsets")) {
            out = acpsess_expand_acp_enabled_toolsets(json_obj_get(in, "toolsets"),
                                                      json_obj_get(in, "mcp_names"));
        } else if (!strcmp(func, "session_info")) {
            out = acpsess_session_info(json_get_str(in, "sid", ""),
                                       json_get_str(in, "cwd", ""),
                                       json_obj_get(in, "model"),
                                       json_get_num(in, "history_len", 0),
                                       json_obj_get(in, "title"), json_obj_get(in, "preview"),
                                       json_obj_get(in, "updated_at"));
        } else if (!strcmp(func, "first_user_preview")) {
            out = acpsess_first_user_preview(json_obj_get(in, "history"),
                                             json_get_str(in, "default_val", ""));
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
