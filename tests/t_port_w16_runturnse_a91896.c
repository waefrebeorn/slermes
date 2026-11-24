#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_runturnse_a91896.h"

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
        if (!strcmp(func, "busy_notice")) {
            out = w16_runturnse_a91896_pending_setup_busy_notice(
                json_get_bool(in, "enabled", 0) ? 1 : 0,
                json_get_bool(in, "allowed", 0) ? 1 : 0,
                json_get_num(in, "timeout", 300.0),
                json_get_num(in, "started", 0.0),
                json_get_num(in, "now", 0.0),
                json_get_num(in, "busy_ack_ts", 0.0),
                json_get_bool(in, "agent_is_pending", 0) ? 1 : 0,
                json_get_bool(in, "state_is_none", 0) ? 1 : 0);
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
