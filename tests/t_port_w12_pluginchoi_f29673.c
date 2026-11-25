/* t_port_w12_pluginchoi_f29673.c — JSON-lines oracle harness for hermes_cli/plugin_choices.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_pluginchoi_f29673.h"

static void emit_key(const char *key, json_t *v)
{
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void)
{
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "options")) {
            out = json_array();
            w12_pluginchoi_f29673_options(json_obj_get(in, "choices"), out);
        } else if (!strcmp(func, "ask_choice")) {
            char buf[2048];
            bool found = w12_pluginchoi_f29673_ask_choice(json_obj_get(in, "choices"),
                                        json_get_str(in, "picked", ""),
                                        buf, sizeof(buf));
            out = found ? json_string(buf) : json_null();
        } else if (!strcmp(func, "clarify_ask")) {
            char buf[2048];
            bool found = w12_pluginchoi_f29673_clarify_ask(json_obj_get(in, "reply"), buf, sizeof(buf));
            out = found ? json_string(buf) : json_null();
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
