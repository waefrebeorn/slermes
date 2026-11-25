/* t_port_w8_sessionloca_b67e17.c — harness for src/gateway/session_local.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessionloca_b67e17.h"

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
        if (!strcmp(func, "effort_fields")) {
            out = w8_sessionloca_b67e17_effort_fields(
                json_obj_get(in, "config"), json_obj_get(in, "tier"),
                json_obj_get(in, "model"), json_obj_get(in, "provider"),
                json_obj_get(in, "base_url"), json_obj_get(in, "api_mode"),
                json_obj_get(in, "supported_efforts"), json_obj_get(in, "static_tiers"),
                json_obj_get(in, "fast_override"));
        } else if (!strcmp(func, "identity")) {
            out = w8_sessionloca_b67e17_identity(json_obj_get(in, "source"));
        }
        printf("{");
        json_t *fv = json_string(func);
        emit_key("func", fv);
        json_free(fv);
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
