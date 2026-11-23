/* Port gate harness for tui_gateway/server.py (gate w6_server_8d971d). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_server_8d971d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "ok")) {
            out = tuisrv_ok(json_obj_get(in, "rid"), json_obj_get(in, "result"));
        } else if (!strcmp(func, "err")) {
            out = tuisrv_err(json_obj_get(in, "rid"), json_get_num(in, "code", 0),
                             json_get_str(in, "msg", ""), json_obj_get(in, "data"));
        } else if (!strcmp(func, "normalize_request")) {
            out = tuisrv_normalize_request(json_obj_get(in, "req"));
        } else if (!strcmp(func, "is_model_switch_marker")) {
            out = tuisrv_is_model_switch_marker(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "is_pivot_marker")) {
            out = tuisrv_is_pivot_marker(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "coerce_int")) {
            out = tuisrv_coerce_int_config_value(json_obj_get(in, "value"),
                                                 json_get_num(in, "default", 0),
                                                 json_get_num(in, "min_value", 0));
        } else if (!strcmp(func, "cli_exec_blocked")) {
            out = tuisrv_cli_exec_blocked(json_obj_get(in, "argv"));
        } else if (!strcmp(func, "turn_started_at")) {
            out = tuisrv_turn_started_at(json_obj_get(in, "session"));
        } else if (!strcmp(func, "event_frame")) {
            out = tuisrv_event_frame(json_get_str(in, "event", ""),
                                     json_get_str(in, "sid", ""),
                                     json_obj_get(in, "payload"));
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
