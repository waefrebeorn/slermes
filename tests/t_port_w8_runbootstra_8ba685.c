/* t_port_w8_runbootstra_8ba685.c — harness for src/gateway/run_bootstrap.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_runbootstra_8ba685.h"

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
        if (!strcmp(func, "identify_payload")) {
            out = w8_runbootstra_8ba685_identify_payload(
                json_obj_get(in, "base"), json_obj_get(in, "descriptor"),
                json_obj_get(in, "draining"),
                json_get_str(in, "restart_wait_reason", "restart_wait"));
        } else if (!strcmp(func, "browse_reconcile_reserve_wait_s")) {
            out = w8_runbootstra_8ba685_browse_reconcile_reserve_wait_s(
                json_obj_get(in, "budget"));
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
