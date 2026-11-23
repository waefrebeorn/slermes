/* t_port_w13_pluginmess_f27478.c — Oracle harness for hermes_cli/plugin_messages.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginmess_f27478.h"
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
        if (!strcmp(func, "err")) {
            out = w13_pluginmess_f27478_err(json_obj_get(in, "code"), json_obj_get(in, "detail"));
        } else if (!strcmp(func, "props")) {
            out = w13_pluginmess_f27478_props(json_obj_get(in, "state"));
        } else if (!strcmp(func, "repr")) {
            out = w13_pluginmess_f27478_repr(json_obj_get(in, "state"));
        } else if (!strcmp(func, "refused")) {
            out = w13_pluginmess_f27478_refused(json_obj_get(in, "state"));
        } else if (!strcmp(func, "settle")) {
            out = w13_pluginmess_f27478_settle(json_obj_get(in, "state"), json_obj_get(in, "result"));
        } else if (!strcmp(func, "adopt")) {
            out = w13_pluginmess_f27478_adopt(json_obj_get(in, "state"), json_obj_get(in, "other"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
