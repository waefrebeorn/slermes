/* t_port_w11_ttstoolli_234137.c - harness for tools/tts_tool_lifecycle.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_ttstoolli_234137.h"

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
        if (!strcmp(func, "keep_warm")) {
            out = w11_ttstoolli_234137_keep_warm_seconds(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "release")) {
            out = w11_ttstoolli_234137_release_provider(json_obj_get(in, "provider"),
                     json_obj_get(in, "cache_sizes"));
        } else if (!strcmp(func, "warm")) {
            out = w11_ttstoolli_234137_warm_noop(json_obj_get(in, "provider"));
        } else if (!strcmp(func, "lease")) {
            out = w11_ttstoolli_234137_lease_op(json_get_str(in, "op", "holders"),
                    json_get_str(in, "lease", ""), json_obj_get(in, "leases"),
                    json_obj_get(in, "cache_sizes"), json_get_num(in, "keep_warm", 60.0));
        } else if (!strcmp(func, "warmers")) {
            out = w11_ttstoolli_234137_warmer_names();
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
