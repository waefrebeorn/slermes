/* t_port_w13_autherror_74a9bf.c — oracle harness for
 * hermes_cli/auth_error_copy.py (gate w13_autherror_74a9bf). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_autherror_74a9bf.h"

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

        if (!strcmp(func, "device_flow_error")) {
            out = w13_autherror_74a9bf_device_flow_error(
                json_get_str(in, "code", ""),
                json_get_str(in, "desc", ""),
                json_get_str(in, "retry", "hermes portal"));
        } else if (!strcmp(func, "is_network_error")) {
            int r = w13_autherror_74a9bf_is_network_error(
                json_get_str(in, "type", ""),
                json_get_str(in, "oauth", ""),
                json_get_str(in, "code", ""));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "is_device_code_expired")) {
            int r = w13_autherror_74a9bf_is_device_code_expired(
                json_get_str(in, "oauth", ""), json_get_str(in, "code", ""));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "is_cancelled")) {
            long long ec = strtoll(json_get_str(in, "exit", "-1"), NULL, 10);
            int r = w13_autherror_74a9bf_is_cancelled(json_get_str(in, "type", ""), ec);
            out = json_bool(r != 0);
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
