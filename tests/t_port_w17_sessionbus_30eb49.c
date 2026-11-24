/* t_port_w17_sessionbus_30eb49.c — JSON-lines harness for the faithful C11
 * port of gateway/session_busy_controls.py (gate w17_sessionbus_30eb49). */
#include "hermes_json.h"
#include "gateway/session_busy_controls.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        json_t *conn = json_copy(json_obj_get(in, "connection"));
        json_t *io = json_copy(json_obj_get(in, "io"));
        const char *sid = json_get_str(in, "session_id", "");
        json_t *params = json_obj_get(in, "params");
        bool write = json_get_bool(in, "write", false);
        json_t *state = json_object();
        json_t *ret = NULL;

        if (!strcmp(func, "handlers")) {
            ret = w17_sessionbus_30eb49_handlers();
        } else if (!strcmp(func, "authorize")) {
            ret = w17_sessionbus_30eb49_authorize(conn, sid, params,
                    json_get_str(in, "capability", ""), io, state);
        } else if (!strcmp(func, "verbose_config")) {
            ret = w17_sessionbus_30eb49_verbose_config(conn, sid, params, write, io, state);
        } else if (!strcmp(func, "show_reasoning_config")) {
            ret = w17_sessionbus_30eb49_show_reasoning_config(conn, sid, params, write, io, state);
        } else if (!strcmp(func, "service_tier_config")) {
            ret = w17_sessionbus_30eb49_service_tier_config(conn, sid, params, io, state);
        } else if (!strcmp(func, "yolo_config")) {
            ret = w17_sessionbus_30eb49_yolo_config(conn, sid, params, write, io, state);
        } else if (!strcmp(func, "adopts_owner_agent")) {
            ret = w17_sessionbus_30eb49_adopts_owner_agent(json_obj_get(conn, "authority"), sid, io, state);
        } else if (!strcmp(func, "correct")) {
            ret = w17_sessionbus_30eb49_correct(conn, sid, params, json_get_str(in, "verb", ""), io, state);
        } else if (!strcmp(func, "busy_config")) {
            ret = w17_sessionbus_30eb49_busy_config(conn, sid, params, write, io, state);
        } else {
            ret = json_string("UNKNOWN_FUNC");
        }

        json_t *out = json_object();
        json_set(out, "ret", ret ? ret : json_null());
        json_set(out, "state", state);
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
        json_free(conn);
        json_free(io);
        json_free(in);
    }
    return 0;
}
