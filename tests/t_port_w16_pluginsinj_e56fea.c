/* JSON-lines harness for the w16_pluginsinj_e56fea gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_pluginsinj_e56fea.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_opt(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL, *origin;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "inject")) {
            origin = json_obj_get(in, "origin");
            if (origin && origin->type == JSON_NULL) origin = NULL;
            out = w16_pluginsinj_e56fea_inject(
                json_get_str(in, "content", ""),
                json_get_str(in, "role", "user"),
                json_get_bool(in, "has_cli", 0) ? 1 : 0,
                json_get_bool(in, "agent_running", 0) ? 1 : 0,
                str_opt(in, "session_key"),
                origin,
                json_get_bool(in, "has_gw_injector", 0) ? 1 : 0,
                json_get_bool(in, "has_tui_injector", 0) ? 1 : 0,
                json_get_bool(in, "injection_allowed", 0) ? 1 : 0,
                json_get_bool(in, "tui_result", 0) ? 1 : 0,
                json_get_bool(in, "tui_raises", 0) ? 1 : 0,
                json_get_bool(in, "gw_result", 0) ? 1 : 0,
                json_get_bool(in, "gw_raises", 0) ? 1 : 0,
                json_get_str(in, "calling_session_key", ""),
                json_get_str(in, "plugin_home", ""));
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
