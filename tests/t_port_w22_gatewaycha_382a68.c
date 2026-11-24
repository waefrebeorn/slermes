/* JSON-lines harness for the w22_gatewaycha_382a68 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_gatewaycha_382a68.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int as_bool(const json_t *in, const char *key) {
    return json_get_bool(in, key, 0) ? 1 : 0;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "ensure")) {
            out = w22_gatewaycha_382a68_ensure_launch_provider(as_bool(in, "bypass"),
                                                               json_obj_get(in, "args"),
                                                               json_get_str(in, "gateway_url", NULL),
                                                               as_bool(in, "provider"));
        } else if (!strcmp(func, "launch")) {
            int code = (int)json_get_num(in, "code", 0);
            out = w22_gatewaycha_382a68_launch_gateway_chat(as_bool(in, "bypass"),
                                                            json_get_bool(in, "import_ok", 1) ? 1 : 0,
                                                            as_bool(in, "hint_handled"),
                                                            (long)code);
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
