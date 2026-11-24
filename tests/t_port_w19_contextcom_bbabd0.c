/* t_port_w19_contextcom_bbabd0.c - harness for src/agent/context_compressor_window.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_contextcom_bbabd0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "summary_view")) {
            out = w19_contextcom_bbabd0_summary_view(json_obj_get(in, "plan"),
                    json_obj_get(in, "originals"),
                    (int)json_get_num(in, "start", 0.0), (int)json_get_num(in, "end", 0.0));
        } else if (!strcmp(func, "restart_probe_bounds")) {
            out = w19_contextcom_bbabd0_restart_probe_bounds(json_obj_get(in, "messages"),
                    (int)json_get_num(in, "protect_first_n", 0.0), (int)json_get_num(in, "extra", 0.0));
        } else if (!strcmp(func, "effective_protect_first_n")) {
            out = json_number(w19_contextcom_bbabd0_effective_protect_first_n(
                    json_get_bool(in, "pin_task_turn", false) ? 1 : 0,
                    (int)json_get_num(in, "compression_count", 0.0),
                    json_get_str(in, "previous_summary", "")[0] != 0,
                    (int)json_get_num(in, "protect_first_n", 0.0),
                    json_obj_get(in, "messages"), (int)json_get_num(in, "extra", 0.0)));
        } else if (!strcmp(func, "protect_head_size")) {
            out = json_number(w19_contextcom_bbabd0_protect_head_size(
                    json_get_bool(in, "pin_task_turn", false) ? 1 : 0,
                    (int)json_get_num(in, "compression_count", 0.0),
                    json_get_str(in, "previous_summary", "")[0] != 0,
                    (int)json_get_num(in, "protect_first_n", 0.0),
                    json_obj_get(in, "messages"), (int)json_get_num(in, "extra", 0.0)));
        } else if (!strcmp(func, "compress_window")) {
            out = w19_contextcom_bbabd0_compress_window(json_obj_get(in, "messages"),
                    json_get_bool(in, "pin_task_turn", false) ? 1 : 0,
                    (int)json_get_num(in, "compression_count", 0.0),
                    json_get_str(in, "previous_summary", "")[0] != 0,
                    (int)json_get_num(in, "protect_first_n", 0.0),
                    (int)json_get_num(in, "extra", 0.0),
                    (int)json_get_num(in, "tail_cut", 0.0),
                    (int)json_get_num(in, "last_user_idx", -1.0),
                    (int)json_get_num(in, "aligned_backward", 0.0),
                    (int)json_get_num(in, "floor", 0.0));
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
