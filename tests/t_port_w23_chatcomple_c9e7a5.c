/* JSON-lines harness for the w23_chatcomple_c9e7a5 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_chatcomple_c9e7a5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
}

static json_t *string_out(char *take) {
    json_t *o = json_string(take ? take : "");
    free(take);
    return o;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out, *world;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        world = json_obj_get(in, "world");
        if (!strcmp(func, "wait_text")) {
            out = string_out(w23_chatcomple_c9e7a5_wait_notice_text(world));
        } else if (!strcmp(func, "deadline")) {
            out = w23_chatcomple_c9e7a5_codex_watchdog_deadline(world);
        } else if (!strcmp(func, "should_emit")) {
            out = w23_chatcomple_c9e7a5_should_emit(world);
        } else if (!strcmp(func, "near")) {
            out = json_bool(w23_chatcomple_c9e7a5_near_deadline(world) ? 1 : 0);
        } else if (!strcmp(func, "phases")) {
            out = w23_chatcomple_c9e7a5_phases();
        } else if (!strcmp(func, "near_secs")) {
            out = json_number(w23_chatcomple_c9e7a5_near_secs());
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
        json_free(in);
    }
    return 0;
}
