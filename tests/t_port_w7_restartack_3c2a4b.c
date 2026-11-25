#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_restartack_3c2a4b.h"

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
        if (!strcmp(func, "send_restart_ack")) {
            int r = w7_restartack_3c2a4b_send_restart_ack(
                json_get_bool(in, "adapter_none", false) ? 1 : 0,
                json_get_bool(in, "platform_local", false) ? 1 : 0,
                json_get_bool(in, "send_raises", false) ? 1 : 0,
                json_get_bool(in, "result_success", false) ? 1 : 0,
                json_get_bool(in, "result_retryable", false) ? 1 : 0,
                json_get_bool(in, "error_retryable", false) ? 1 : 0,
                json_get_bool(in, "error_timeout", false) ? 1 : 0);
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
