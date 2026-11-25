/* JSON-lines harness for the w21_refreshsin_e3dbfd gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_refreshsin_e3dbfd.h"
#include "hermes_cli/dashboard_auth/refresh_singleflight.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "digest")) {
            char *s = w21_refreshsin_e3dbfd_digest(json_get_str(in, "token", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "constants")) {
            out = w21_refreshsin_e3dbfd_constants();
        } else if (!strcmp(func, "forget")) {
            out = w21_refreshsin_e3dbfd_forget(json_obj_get(in, "entries"),
                                               json_obj_get(in, "flights"),
                                               json_get_str(in, "token", ""));
        } else if (!strcmp(func, "prune")) {
            out = w21_refreshsin_e3dbfd_prune(json_obj_get(in, "entries"),
                                              json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "refresh")) {
            out = w21_refreshsin_e3dbfd_refresh(in);
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
