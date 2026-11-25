/* t_port_w18_gatewayrun_844e6c.c — JSON-lines harness for gateway_runtime_cli.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_gatewayrun_844e6c.h"

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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "ensure_exit_code")) {
            out = w18_gatewayrun_844e6c_ensure_exit_code(opt_str(in, "reason_code"),
                                                         opt_str(in, "state"));
        } else if (!strcmp(func, "verdict_payload")) {
            out = w18_gatewayrun_844e6c_verdict_payload(json_obj_get(in, "result"),
                                                        opt_str(in, "code_sha"));
        } else if (!strcmp(func, "constants")) {
            out = w18_gatewayrun_844e6c_invalid_invocation();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
