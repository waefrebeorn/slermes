/* t_port_w12_profilesca_d12947.c — JSON-lines harness for hermes_cli/profiles_cache.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_profilesca_d12947.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "cached_profile_read")) {
            out = w12_profilesca_d12947_cached_profile_read(
                json_get_str(in, "path", ""),
                json_get_str(in, "kind", ""),
                json_obj_get(in, "signature"),
                json_obj_get(in, "value"),
                (long long)json_get_num(in, "max", 512.0),
                json_get_bool(in, "reset", false) ? 1 : 0);
        } else if (!strcmp(func, "yaml_scalar_to_json")) {
            char buf[8192];
            w12_profilesca_d12947_yaml_scalar_to_json(json_obj_get(in, "value"),
                                                      buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "json_safe_dumps")) {
            char *s = w12_profilesca_d12947_json_safe_dumps(json_obj_get(in, "value"));
            out = json_string(s ? s : "");
            free(s);
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
    w12_profilesca_d12947_cache_free();
    return 0;
}
