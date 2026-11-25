#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_browsersup_1c8a2d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int bool_or(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return def;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "invalid_message")) {
            char *s = w19_browsersup_1c8a2d_invalid_message(json_get_str(in, "task_id", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "is_valid")) {
            out = w19_browsersup_1c8a2d_is_valid(bool_or(in, "registry_same", 1),
                                                 bool_or(in, "active", 1),
                                                 bool_or(in, "stop_requested", 0),
                                                 bool_or(in, "ws_same", 1),
                                                 bool_or(in, "loop_running", 1));
        } else if (!strcmp(func, "call_gate")) {
            const char *bs = json_get_str(in, "before_send", "none");
            int kind = !strcmp(bs, "callable") ? 1 : (!strcmp(bs, "notcallable") ? 2 : 0);
            out = w19_browsersup_1c8a2d_call_gate(json_obj_get(in, "timeout"),
                                                  json_get_num(in, "tmax", 4294967.0),
                                                  kind,
                                                  bool_or(in, "valid", 1),
                                                  bool_or(in, "loop_misuse", 0),
                                                  json_get_str(in, "method", "Runtime.evaluate"),
                                                  json_get_str(in, "task_id", "t1"));
        } else if (!strcmp(func, "capture_error")) {
            out = w19_browsersup_1c8a2d_capture_error(bool_or(in, "found", 1),
                                                      bool_or(in, "loop_present", 1),
                                                      bool_or(in, "loop_running", 1),
                                                      bool_or(in, "active", 1),
                                                      bool_or(in, "ws_present", 1),
                                                      bool_or(in, "page_present", 1),
                                                      bool_or(in, "schedule_unavailable", 0),
                                                      json_get_str(in, "task_id", "t1"));
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
