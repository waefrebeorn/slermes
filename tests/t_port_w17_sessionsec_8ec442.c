/* t_port_w17_sessionsec_8ec442.c — JSON-lines harness for gateway/session_secret_prompt.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_sessionsec_8ec442.h"

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
        if (!strcmp(func, "skipped"))
            out = w17_sessionsec_8ec442_skipped(json_get_str(in, "var_name", ""),
                        json_get_str(in, "reason", ""), json_get_str(in, "message", ""));
        else if (!strcmp(func, "callback_result")) {
            const json_t *av = json_obj_get(in, "ask_value");
            const char *ask = (av && av->type == JSON_STRING) ? av->str_val : NULL;
            out = w17_sessionsec_8ec442_callback_result(
                        json_get_str(in, "var_name", ""),
                        json_get_str(in, "prompt", ""),
                        json_get_bool(in, "has_metadata", false) ? 1 : 0,
                        json_get_str(in, "metadata", NULL),
                        json_get_bool(in, "live_session", false) ? 1 : 0,
                        ask,
                        json_obj_get(in, "saved"));
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
