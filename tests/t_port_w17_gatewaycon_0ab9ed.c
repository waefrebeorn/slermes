/* t_port_w17_gatewaycon_0ab9ed.c — JSON-lines harness for gateway_connection.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_gatewaycon_0ab9ed.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "lost_before")) {
            out = w17_gatewaycon_0ab9ed_lost_before(
                    json_get_bool(in, "is_exception", 0) ? 1 : 0,
                    opt_str(in, "connection_epoch"), opt_str(in, "epoch"));
        } else if (!strcmp(func, "runs_route")) {
            out = w17_gatewaycon_0ab9ed_runs_route(
                    json_obj_get(in, "info"), opt_str(in, "model"), opt_str(in, "provider"));
        } else if (!strcmp(func, "apply_event")) {
            out = w17_gatewaycon_0ab9ed_apply_event(
                    opt_str(in, "kind"), json_obj_get(in, "payload"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
