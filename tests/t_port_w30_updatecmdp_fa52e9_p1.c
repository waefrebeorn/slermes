/* JSON-lines harness for the w30_updatecmdp_fa52e9_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_updatecmdp_fa52e9_p1.h"

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
        if (!strcmp(func, "empty_token")) {
            out = w30_updatecmdp_fa52e9_p1_empty_token();
        } else if (!strcmp(func, "gateways_paused")) {
            out = w30_updatecmdp_fa52e9_p1_gateways_paused(in);
        } else if (!strcmp(func, "inside")) {
            out = w30_updatecmdp_fa52e9_p1_inside(in);
        } else if (!strcmp(func, "scope_cmd")) {
            out = w30_updatecmdp_fa52e9_p1_scope_cmd(in);
        } else if (!strcmp(func, "manage_cmd")) {
            out = w30_updatecmdp_fa52e9_p1_manage_cmd(in);
        } else if (!strcmp(func, "unit_stop_pending")) {
            out = w30_updatecmdp_fa52e9_p1_unit_stop_pending(in);
        } else if (!strcmp(func, "alive")) {
            out = w30_updatecmdp_fa52e9_p1_alive(in);
        } else if (!strcmp(func, "is_left_pending")) {
            out = w30_updatecmdp_fa52e9_p1_is_left_pending(in);
        } else if (!strcmp(func, "adopt_left_pending")) {
            out = w30_updatecmdp_fa52e9_p1_adopt_left_pending(in);
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
