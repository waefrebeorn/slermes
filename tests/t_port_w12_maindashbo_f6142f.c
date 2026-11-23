#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_maindashbo_f6142f.h"

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
        char *s = NULL;
        if (!strcmp(func, "probe_host")) {
            s = w12_maindashbo_f6142f_dashboard_probe_host(json_get_str(in, "host", NULL));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "scope")) {
            s = w12_maindashbo_f6142f_extract_scope_from_cgroup(json_get_str(in, "entry", NULL));
            out = s ? json_string(s) : json_null();
        } else if (!strcmp(func, "restart_commands")) {
            out = w12_maindashbo_f6142f_systemd_restart_commands(
                json_get_str(in, "unit", ""), json_get_str(in, "scope", NULL));
        } else if (!strcmp(func, "format_restart")) {
            s = w12_maindashbo_f6142f_format_restart_commands(json_obj_get(in, "commands"));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "endpoint_flags")) {
            out = w12_maindashbo_f6142f_explicit_endpoint_flags(json_obj_get(in, "argv"));
        } else if (!strcmp(func, "electron_dist")) {
            out = json_bool(w12_maindashbo_f6142f_is_electron_packaged_web_dist(
                json_get_str(in, "path", NULL)) ? 1 : 0);
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
        free(s);
        json_free(out);
        json_free(in);
    }
    return 0;
}
