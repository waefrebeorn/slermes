/* t_port_w13_clienterb_07c762.c — oracle harness for
 * hermes_cli/cli_enter_burst.py (gate w13_clienterb_07c762). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_clienterb_07c762.h"

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

        if (!strcmp(func, "note_text_change")) {
            out = w13_clienterb_07c762_note_text_change(
                json_get_num(in, "now", 0.0),
                json_get_num(in, "last", 0.0),
                json_get_num(in, "burst", -1e9),
                json_get_bool(in, "held", false) ? 1 : 0);
        } else if (!strcmp(func, "handle_burst_enter")) {
            out = w13_clienterb_07c762_handle_burst_enter(
                json_get_str(in, "doc", "hello"),
                json_get_num(in, "now", 0.0),
                json_get_num(in, "last", 0.0),
                json_get_num(in, "burst", -1e9),
                json_get_bool(in, "held", false) ? 1 : 0,
                json_get_bool(in, "has_kp", true) ? 1 : 0,
                json_get_bool(in, "has_loop", true) ? 1 : 0,
                json_get_bool(in, "queue", false) ? 1 : 0,
                json_get_bool(in, "running", true) ? 1 : 0,
                json_get_bool(in, "settle", false) ? 1 : 0,
                json_get_bool(in, "clear_held", false) ? 1 : 0,
                json_get_bool(in, "edit_doc", false) ? 1 : 0,
                json_get_bool(in, "change_last", false) ? 1 : 0);
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
