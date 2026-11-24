/* JSON-lines harness for the w20_server_266a25 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_server_266a25.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "start_bot")) {
            out = w20_server_266a25_start_bot(json_obj_get(in, "payload"),
                                              json_obj_get(in, "start_result"));
        } else if (!strcmp(func, "say")) {
            out = w20_server_266a25_say(json_obj_get(in, "payload"),
                                        json_obj_get(in, "active"),
                                        json_get_bool(in, "write_ok", 1) ? 1 : 0);
        } else if (!strcmp(func, "dispatch")) {
            out = w20_server_266a25_dispatch(json_get_str(in, "type", ""));
        } else if (!strcmp(func, "pong")) {
            out = w20_server_266a25_pong(in, json_get_str(in, "display_name", ""),
                                         json_get_num(in, "ts", 0.0));
        } else if (!strcmp(func, "error_envelope")) {
            out = w20_server_266a25_error_envelope(json_get_str(in, "id", ""),
                                                   json_get_str(in, "reason", ""));
        } else if (!strcmp(func, "ensure_token")) {
            out = w20_server_266a25_ensure_token(json_obj_get(in, "token_data"),
                                                 json_obj_get(in, "cached"));
        } else if (!strcmp(func, "start_bot_keys")) {
            out = w20_server_266a25_start_bot_keys();
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
