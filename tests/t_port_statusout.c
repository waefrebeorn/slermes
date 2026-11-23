#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_statusout.h"

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
        int r = 0;
        if (!strcmp(func, "quiet_tool_messages")) {
            r = statusout_should_emit_quiet_tool_messages(
                    json_get_bool(in, "suppress", false) ? 1 : 0,
                    json_get_bool(in, "quiet_mode", false) ? 1 : 0,
                    json_get_bool(in, "tool_progress_cb", false) ? 1 : 0,
                    json_get_str(in, "platform", ""));
        } else if (!strcmp(func, "quiet_spinner")) {
            r = statusout_should_start_quiet_spinner(
                    json_get_bool(in, "has_print_fn", false) ? 1 : 0,
                    json_get_bool(in, "isatty", false) ? 1 : 0);
        } else if (!strcmp(func, "safe_print")) {
            r = statusout_safe_print(
                    json_get_bool(in, "mute_notification", false) ? 1 : 0,
                    json_get_bool(in, "diagnostic", false) ? 1 : 0,
                    json_get_bool(in, "warning_enabled", true) ? 1 : 0);
        } else if (!strcmp(func, "vprint")) {
            r = statusout_vprint(
                    json_get_bool(in, "suppress", false) ? 1 : 0,
                    json_get_bool(in, "diagnostic", false) ? 1 : 0,
                    json_get_bool(in, "warning_enabled", true) ? 1 : 0,
                    json_get_bool(in, "force", false) ? 1 : 0,
                    json_get_bool(in, "mute_post", false) ? 1 : 0,
                    json_get_bool(in, "has_consumers", false) ? 1 : 0,
                    json_get_bool(in, "executing", false) ? 1 : 0,
                    json_get_bool(in, "mute_notification", false) ? 1 : 0);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        if (!out) out = json_bool(r ? true : false);
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
