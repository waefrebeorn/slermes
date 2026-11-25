/* JSON-lines harness for the w30_hostedroom_fe22db_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_hostedroom_fe22db_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, const json_t *in) {
    if (!strcmp(func, "constants")) return w30_hostedroom_fe22db_p2_constants(in);
    if (!strcmp(func, "manifest_validate")) return w30_hostedroom_fe22db_p2_manifest_validate(in);
    if (!strcmp(func, "retain")) return w30_hostedroom_fe22db_p2_retain(in);
    if (!strcmp(func, "require_viewer_room"))
        return w30_hostedroom_fe22db_p2_require_viewer_room(in);
    if (!strcmp(func, "read_gate")) return w30_hostedroom_fe22db_p2_read_gate(in);
    if (!strcmp(func, "range_slice")) return w30_hostedroom_fe22db_p2_range_slice(in);
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(func, in);
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
