#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_usermessag_547811.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *ret_str(char *s) {
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
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
        int rec = json_get_bool(in, "recoverable", true) ? 1 : 0;
        const json_t *surface = json_obj_get(in, "surface");
        if (!strcmp(func, "identity")) {
            out = ret_str(w13_usermessag_547811_identity(surface));
        } else if (!strcmp(func, "title")) {
            out = ret_str(w13_usermessag_547811_turn_error_title(surface));
        } else if (!strcmp(func, "hint")) {
            out = ret_str(w13_usermessag_547811_turn_error_hint(surface, rec));
        } else if (!strcmp(func, "text")) {
            out = ret_str(w13_usermessag_547811_turn_error_text(
                json_get_str(in, "error", ""), surface, rec));
        } else if (!strcmp(func, "busy")) {
            out = ret_str(w13_usermessag_547811_busy_message(
                json_get_str(in, "command", ""),
                json_get_bool(in, "compressing", false) ? 1 : 0));
        } else if (!strcmp(func, "agent_init")) {
            out = ret_str(w13_usermessag_547811_agent_init_failed_message(
                json_get_str(in, "exc", "")));
        } else if (!strcmp(func, "resume_failed")) {
            out = ret_str(w13_usermessag_547811_resume_failed_message(
                json_get_str(in, "exc", "")));
        } else if (!strcmp(func, "still_starting")) {
            out = ret_str(w13_usermessag_547811_agent_still_starting());
        } else if (!strcmp(func, "build_abandoned")) {
            out = ret_str(w13_usermessag_547811_agent_build_abandoned());
        } else if (!strcmp(func, "missing_for_turn")) {
            out = ret_str(w13_usermessag_547811_agent_missing_for_turn());
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
