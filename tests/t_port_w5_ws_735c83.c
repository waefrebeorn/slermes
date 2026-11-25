#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_ws_735c83.h"

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
        if (!strcmp(func, "peer")) {
            char *p = w5_ws_735c83_peer_label(json_obj_get(in, "client"));
            out = p ? json_string(p) : json_null();
            free(p);
        } else if (!strcmp(func, "unknown")) {
            out = json_bool(w5_ws_735c83_is_unknown_method(json_obj_get(in, "resp")) ? true : false);
        } else if (!strcmp(func, "concurrent")) {
            out = json_bool(w5_ws_735c83_is_concurrent_read(json_get_str(in, "method", "")) ? true : false);
        } else if (!strcmp(func, "streaming")) {
            out = json_bool(w5_ws_735c83_is_streaming_event(json_get_str(in, "event", "")) ? true : false);
        } else if (!strcmp(func, "answers")) {
            out = json_bool(w5_ws_735c83_answers_open_request(
                    json_obj_get(in, "req"),
                    json_get_bool(in, "is_response", false) ? 1 : 0) ? true : false);
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
