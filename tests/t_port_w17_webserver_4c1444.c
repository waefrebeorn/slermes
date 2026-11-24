#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_webserver_4c1444.h"

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

        if (!strcmp(func, "build")) {
            long long build = 0;
            if (w17_webserver_4c1444_windows_build_number(
                    json_get_str(in, "version", ""), json_get_str(in, "platform_label", ""), &build))
                out = json_number((double)build);
            else
                out = json_null();
        } else if (!strcmp(func, "display")) {
            out = w17_webserver_4c1444_display_system_platform(
                json_get_str(in, "system", ""), json_get_str(in, "release", ""),
                json_get_str(in, "version", ""), json_get_str(in, "platform_label", ""));
        } else if (!strcmp(func, "named")) {
            char *s = w17_webserver_4c1444_named_profile_from_action(json_obj_get(in, "subcommand"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "hostspawn")) {
            out = json_bool(w17_webserver_4c1444_is_host_gateway_spawn(json_obj_get(in, "subcommand")));
        } else if (!strcmp(func, "split")) {
            out = w17_webserver_4c1444_split_text_for_speak_stream(
                json_get_str(in, "text", ""), (long long)json_get_num(in, "cap", 4000));
        } else if (!strcmp(func, "strip")) {
            out = w17_webserver_4c1444_strip_session_list_rows(json_obj_get(in, "sessions"));
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
