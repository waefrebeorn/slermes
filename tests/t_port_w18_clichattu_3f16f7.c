#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_clichattu_3f16f7.h"

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
        if (!strcmp(func, "chat_resolve_interrupt")) {
            json_t *result = json_obj_get(in, "result");
            if (result && result->type != JSON_OBJECT) result = NULL;
            const char *imsg = json_get_str(in, "interrupt_msg", NULL);
            const char *resp = json_get_str(in, "response", "");
            int alive = json_get_bool(in, "thread_alive", false) ? 1 : 0;
            int req = json_get_bool(in, "interrupt_requested", false) ? 1 : 0;
            out = w18_clichattu_3f16f7_chat_resolve_interrupt(result, imsg, resp, alive, req);
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
