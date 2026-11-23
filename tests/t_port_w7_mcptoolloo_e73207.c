/* t_port_w7_mcptoolloo_e73207.c -- JSON-lines harness for src/tools/mcp_tool_loop.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_mcptoolloo_e73207.h"

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
        if (!strcmp(func, "forward")) {
            out = json_bool(w7_mcptoolloo_e73207_should_forward(
                get_flag(in, "exc_is_runtime"), json_get_str(in, "message", "")) != 0);
        } else if (!strcmp(func, "wait_ready")) {
            out = json_bool(w7_mcptoolloo_e73207_wait_ready(
                json_get_num(in, "timeout", 0.75), json_obj_get(in, "script")) != 0);
        } else if (!strcmp(func, "signal")) {
            out = json_number((double)w7_mcptoolloo_e73207_signal_reconnect(
                get_flag(in, "has_event"), get_flag(in, "event_is_asyncio"),
                get_flag(in, "loop_running")));
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
