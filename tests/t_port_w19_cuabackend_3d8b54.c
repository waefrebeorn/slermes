#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_cuabackend_3d8b54.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int req_flag(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return def;
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
        if (!strcmp(func, "refuse")) {
            json_t *code = json_obj_get(in, "code");
            json_t *dm = json_obj_get(in, "delivery_mode");
            out = w19_cuabackend_3d8b54_refuse(
                json_get_str(in, "action", ""), json_get_str(in, "message", ""),
                (code && code->type == JSON_STRING) ? code->str_val : NULL,
                (dm && dm->type == JSON_STRING) ? dm->str_val : NULL);
        } else if (!strcmp(func, "target_args")) {
            out = w19_cuabackend_3d8b54_target_args(
                json_get_str(in, "action", ""), json_obj_get(in, "pid"), json_obj_get(in, "window_id"),
                req_flag(in, "need_window", 0));
        } else if (!strcmp(func, "apply_delivery")) {
            json_t *dm = json_obj_get(in, "delivery_mode");
            json_t *supp = json_obj_get(in, "supports_delivery_mode");
            out = w19_cuabackend_3d8b54_apply_delivery(
                json_get_str(in, "action", ""),
                (dm && dm->type == JSON_STRING) ? dm->str_val : NULL,
                (supp && supp->type == JSON_BOOL) ? supp->bool_val : 0);
        } else if (!strcmp(func, "parse_key_combo")) {
            out = w19_cuabackend_3d8b54_parse_key_combo(json_get_str(in, "keys", ""));
        } else if (!strcmp(func, "run_input_action")) {
            out = w19_cuabackend_3d8b54_run_input_action(in);
        } else if (!strcmp(func, "click")) {
            out = w19_cuabackend_3d8b54_click(in);
        } else if (!strcmp(func, "drag")) {
            out = w19_cuabackend_3d8b54_drag(in);
        } else if (!strcmp(func, "scroll")) {
            out = w19_cuabackend_3d8b54_scroll(in);
        } else if (!strcmp(func, "type_text")) {
            out = w19_cuabackend_3d8b54_type_text(in);
        } else if (!strcmp(func, "key")) {
            out = w19_cuabackend_3d8b54_key(in);
        } else if (!strcmp(func, "set_value")) {
            out = w19_cuabackend_3d8b54_set_value(in);
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
