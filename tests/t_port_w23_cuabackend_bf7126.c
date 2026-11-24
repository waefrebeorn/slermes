/* JSON-lines harness for the w23_cuabackend_bf7126 gate
 * (tools/computer_use/cua_backend_capture.py).  One request object per stdin
 * line; one response line carrying the computed "out" plus every input key. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_cuabackend_bf7126.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) {
        fputs(s, stdout);
        free(s);
    } else {
        fputs("null", stdout);
    }
}

static json_t *out_string(char *s) {
    json_t *n = json_string(s ? s : "");
    free(s);
    return n;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "self_window")) {
            out = json_bool(w23_cuabackend_bf7126_self_window(json_obj_get(in, "w")) != 0);
        } else if (!strcmp(func, "desktop_window")) {
            out = json_bool(w23_cuabackend_bf7126_desktop_window(json_obj_get(in, "w"),
                                                                json_get_bool(in, "backdrop", 0) ? 1 : 0) != 0);
        } else if (!strcmp(func, "select_target")) {
            out = w23_cuabackend_bf7126_select_target(in);
        } else if (!strcmp(func, "sorted_windows")) {
            out = w23_cuabackend_bf7126_sorted_windows(in);
        } else if (!strcmp(func, "png_metrics")) {
            out = w23_cuabackend_bf7126_png_metrics(in);
        } else if (!strcmp(func, "match_windows")) {
            out = w23_cuabackend_bf7126_match_windows(in);
        } else if (!strcmp(func, "list_apps")) {
            out = w23_cuabackend_bf7126_list_apps(in);
        } else if (!strcmp(func, "is_full_screen")) {
            out = json_bool(w23_cuabackend_bf7126_is_full_screen(json_obj_get(in, "app")) != 0);
        } else if (!strcmp(func, "is_desktop_shell")) {
            out = json_bool(w23_cuabackend_bf7126_is_desktop_shell(json_obj_get(in, "app")) != 0);
        } else if (!strcmp(func, "no_app_match_message")) {
            out = out_string(w23_cuabackend_bf7126_no_app_match_message(json_obj_get(in, "app")));
        } else if (!strcmp(func, "no_desktop_message")) {
            out = out_string(w23_cuabackend_bf7126_no_desktop_message(json_obj_get(in, "app")));
        } else if (!strcmp(func, "constants")) {
            out = w23_cuabackend_bf7126_constants();
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
