#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_clishutdown_5f546e.h"

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
        if (!strcmp(func, "float_env")) {
            json_t *def = json_obj_get(in, "default");
            out = json_number(w9_clishutdown_5f546e_float_env(
                        json_obj_get(in, "env_value"),
                        (def && def->type == JSON_NUMBER) ? def->num_val : 30.0));
        } else if (!strcmp(func, "should_emit")) {
            json_t *gw = json_obj_get(in, "gateway_owns");
            out = json_bool(w9_clishutdown_5f546e_should_emit_cleanup_session_finalize(
                        json_obj_get(in, "session_id"),
                        (gw && gw->type == JSON_BOOL && gw->bool_val) ? 1 : 0,
                        json_obj_get(in, "handed_off"),
                        json_obj_get(in, "attempted")) ? true : false);
        } else if (!strcmp(func, "oneshot_session")) {
            out = w9_clishutdown_5f546e_oneshot_session(
                        json_obj_get(in, "agent_session"), json_obj_get(in, "cli_session"));
        } else
            out = json_string("UNKNOWN_FUNC");
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
