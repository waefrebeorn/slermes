/* Port gate harness for agent/session_source.py (gate w7_sessionsour_f0a029). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_sessionsour_f0a029.h"

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
        if (!strcmp(func, "create_surface")) {
            out = json_string(w7_sessionsour_f0a029_create_surface(json_get_str(in, "source", "")));
        } else if (!strcmp(func, "session_source_for")) {
            char *s = w7_sessionsour_f0a029_session_source_for(
                json_get_str(in, "platform", ""), json_get_str(in, "source", ""),
                json_get_bool(in, "single_query", false), json_get_bool(in, "explicit", false));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "is_ui_transport")) {
            out = json_bool(w7_sessionsour_f0a029_is_ui_transport(json_get_str(in, "source", "")));
        } else if (!strcmp(func, "is_cli_family")) {
            out = json_bool(w7_sessionsour_f0a029_is_cli_family(json_get_str(in, "source", "")));
        } else if (!strcmp(func, "is_daybreak")) {
            out = json_bool(w7_sessionsour_f0a029_is_daybreak(json_get_str(in, "source", "")));
        } else if (!strcmp(func, "is_attended")) {
            out = json_bool(w7_sessionsour_f0a029_is_attended(json_get_str(in, "source", "")));
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
