#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_mcptooleve_88d007.h"

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
        if (!strcmp(func, "events_enabled")) {
            out = json_bool(w8_mcptooleve_88d007_events_enabled(json_obj_get(in, "cfg")) ? true : false);
        } else if (!strcmp(func, "callback_url")) {
            char *s = w8_mcptooleve_88d007_callback_url(
                json_get_str(in, "url_base", ""), json_get_str(in, "route_name", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "event_prompt")) {
            char *s = w8_mcptooleve_88d007_event_prompt(
                json_get_str(in, "server", ""), json_get_str(in, "name", ""),
                json_get_str(in, "instructions", NULL));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "server_routes")) {
            out = w8_mcptooleve_88d007_server_routes(json_obj_get(in, "subscriptions"),
                  json_get_str(in, "server", ""));
        } else if (!strcmp(func, "subscribe_args_error")) {
            char *s = w8_mcptooleve_88d007_subscribe_args_error(json_obj_get(in, "args"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "subscribe_params")) {
            out = w8_mcptooleve_88d007_subscribe_params(
                json_get_str(in, "route_name", ""), json_obj_get(in, "route"),
                json_get_str(in, "url_base", ""), json_obj_get(in, "state"));
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
