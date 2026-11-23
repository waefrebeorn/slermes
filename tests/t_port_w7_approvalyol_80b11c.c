/* t_port_w7_approvalyol_80b11c.c -- JSON-lines harness for src/tools/approval_yolo.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_approvalyol_80b11c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int get_flag(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 0;
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
        if (!strcmp(func, "with_session_yolo")) {
            out = w7_approvalyol_80b11c_with_session_yolo(
                json_obj_get(in, "model_config"), get_flag(in, "enabled"));
        } else if (!strcmp(func, "toggle")) {
            out = json_bool(w7_approvalyol_80b11c_toggle_session_yolo(
                get_flag(in, "has_enabled"), get_flag(in, "enabled"),
                get_flag(in, "persisted"), get_flag(in, "current")) != 0);
        } else if (!strcmp(func, "restore")) {
            out = json_bool(w7_approvalyol_80b11c_restore_session_yolo(
                get_flag(in, "key_empty"), get_flag(in, "persisted"),
                get_flag(in, "frozen"), get_flag(in, "current")) != 0);
        } else if (!strcmp(func, "apply_launch")) {
            out = json_bool(w7_approvalyol_80b11c_apply_launch_yolo(
                get_flag(in, "key_empty"), get_flag(in, "frozen"),
                get_flag(in, "applied")) != 0);
        } else if (!strcmp(func, "transfer")) {
            out = json_bool(w7_approvalyol_80b11c_transfer_session_yolo(
                get_flag(in, "old_empty"), get_flag(in, "new_empty"),
                get_flag(in, "same"), get_flag(in, "old_enabled")) != 0);
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
