/* t_port_w17_sessioning_11caf7.c — JSON-lines harness for gateway/session_ingress_burst.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_sessioning_11caf7.h"

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
        if (!strcmp(func, "holds_busy_burst"))
            out = json_bool(w17_sessioning_11caf7_holds_busy_burst(
                json_obj_get(in, "adapter"), json_get_str(in, "session_key", "")) ? 1 : 0);
        else if (!strcmp(func, "serial"))
            out = json_bool(w17_sessioning_11caf7_serial(json_obj_get(in, "adapter")) ? 1 : 0);
        else if (!strcmp(func, "kind")) {
            const char *k = w17_sessioning_11caf7_kind(json_obj_get(in, "adapter"),
                                                       json_obj_get(in, "event"));
            out = k ? json_string(k) : json_null();
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
