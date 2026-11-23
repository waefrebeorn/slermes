/* t_port_w12_pluginsess_6b33f7.c — JSON-lines oracle harness for hermes_cli/plugin_session.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_pluginsess_6b33f7.h"

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
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const json_t *v;
        char buf[4096];
        json_t *out = NULL;

        if (!strcmp(func, "session_surface")) {
            v = json_obj_get(in, "platform");
            w12_pluginsess_6b33f7_session_surface(v && v->type == JSON_STRING,
                                json_get_str(in, "platform", ""),
                                json_get_str(in, "env_platform", ""),
                                buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "context_surface")) {
            w12_pluginsess_6b33f7_context_surface(json_get_bool(in, "cli_ref", false),
                                json_get_str(in, "surface", ""),
                                buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "session_cwd")) {
            v = json_obj_get(in, "policy_cwd");
            w12_pluginsess_6b33f7_session_cwd(json_get_str(in, "scoped", ""),
                            v && v->type == JSON_STRING,
                            json_get_str(in, "policy_cwd", ""),
                            json_get_str(in, "terminal_cwd", ""),
                            buf, sizeof(buf));
            out = json_string(buf);
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
