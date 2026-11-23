#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_pluginisol_0cd954.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *cfg = opt_str(in, "cfg_isolation");
        const char *env = opt_str(in, "host_env");
        json_t *out = NULL;
        if (!strcmp(func, "isolation_mode")) {
            out = json_string(w15_pluginisol_0cd954_isolation_mode(cfg, env));
        } else if (!strcmp(func, "in_process_import_refusal")) {
            char *s = w15_pluginisol_0cd954_in_process_import_refusal(
                json_get_str(in, "what", ""), json_get_str(in, "source", "user"), cfg, env);
            if (s) { out = json_string(s); free(s); }
            else out = json_null();
        } else if (!strcmp(func, "host_launcher")) {
            out = w15_pluginisol_0cd954_host_launcher(json_obj_get(in, "launcher"));
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
