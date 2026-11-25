/* JSON-lines harness for the w29_adapter_0e0bda_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_0e0bda_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w29_adapter_0e0bda_constants();
        } else if (!strcmp(func, "reply_timeout")) {
            out = w29_adapter_0e0bda_reply_timeout(json_obj_get(in, "env"));
        } else if (!strcmp(func, "orphan_timeout")) {
            out = w29_adapter_0e0bda_orphan_timeout(json_obj_get(in, "env"));
        } else if (!strcmp(func, "clean_slug")) {
            out = w29_adapter_0e0bda_clean_slug(json_obj_get(in, "value"));
        } else if (!strcmp(func, "join_url")) {
            out = w29_adapter_0e0bda_join_url(json_obj_get(in, "base"), json_obj_get(in, "prefix"));
        } else if (!strcmp(func, "safe_context_slug")) {
            out = w29_adapter_0e0bda_safe_context_slug(json_obj_get(in, "value"), json_obj_get(in, "max_len"));
        } else if (!strcmp(func, "server_class")) {
            out = w29_adapter_0e0bda_server_class(json_obj_get(in, "host"));
        } else if (!strcmp(func, "client_ip")) {
            out = w29_adapter_0e0bda_client_ip(json_obj_get(in, "addr"));
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
