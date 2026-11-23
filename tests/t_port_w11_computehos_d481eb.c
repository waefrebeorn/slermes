/* t_port_w11_computehos_d481eb.c - harness for tui_gateway/compute_host.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_computehos_d481eb.h"

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
        if (!strcmp(func, "history_meta")) {
            out = w11_computehos_d481eb_history_meta(json_obj_get(in, "session"));
        } else if (!strcmp(func, "workers")) {
            out = w11_computehos_d481eb_default_workers(json_obj_get(in, "env_value"));
        } else if (!strcmp(func, "transport_write")) {
            out = w11_computehos_d481eb_host_transport_write(json_obj_get(in, "obj"));
        } else if (!strcmp(func, "frame_handler")) {
            out = w11_computehos_d481eb_frame_handler(json_get_str(in, "kind", ""));
        } else if (!strcmp(func, "control_failure")) {
            out = w11_computehos_d481eb_control_failure(json_get_str(in, "route", ""));
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
