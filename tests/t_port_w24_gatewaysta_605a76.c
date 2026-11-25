/* JSON-lines harness for the w24_gatewaysta_605a76 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_gatewaysta_605a76.h"

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
        if (!strcmp(func, "verdict")) {
            out = w24_gatewaysta_605a76_verdict(json_obj_get(in, "observed"),
                                                json_get_bool(in, "raises", false));
        } else if (!strcmp(func, "report_lines")) {
            out = w24_gatewaysta_605a76_report_lines(json_obj_get(in, "observed"),
                                                     json_get_bool(in, "raises", false),
                                                     json_obj_get(in, "pids"));
        } else if (!strcmp(func, "standalone_note")) {
            out = w24_gatewaysta_605a76_standalone_note(json_obj_get(in, "active"),
                                                        json_obj_get(in, "roster"),
                                                        json_obj_get(in, "served"));
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
