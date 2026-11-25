/* t_port_w8_statusserve_cfb190.c -- JSON-lines harness for src/gateway/status_served.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_statusserve_cfb190.h"

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
        if (!strcmp(func, "local_liveness_pid")) {
            out = w8_statusserve_cfb190_local_liveness_pid(json_obj_get(in, "liveness"));
        } else if (!strcmp(func, "served_profile_runtime")) {
            out = w8_statusserve_cfb190_served_profile_runtime(
                json_obj_get(in, "home"), json_obj_get(in, "own_runtime"),
                json_obj_get(in, "gateway_pid"), json_obj_get(in, "seams"));
        } else if (!strcmp(func, "served_profile_mirror_platforms")) {
            out = w8_statusserve_cfb190_served_profile_mirror_platforms(
                json_obj_get(in, "runtime"), json_get_str(in, "profile", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
