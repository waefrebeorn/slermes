/* JSON-lines harness for the w20_windowspro_18c6a5 gate.
 * One request object per stdin line; one response per line with "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_windowspro_18c6a5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
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
        if (!strcmp(func, "dropped_peer")) {
            w20_windowspro_18c6a5_exc exc;
            w20_windowspro_18c6a5_exc_from_json(in, &exc);
            out = json_bool(w20_windowspro_18c6a5_dropped_peer(&exc) ? 1 : 0);
        } else if (!strcmp(func, "should_install")) {
            out = json_bool(w20_windowspro_18c6a5_should_install(
                json_get_bool(in, "is_proactor_loop", 1) ? 1 : 0,
                json_get_bool(in, "has_proactor", 1) ? 1 : 0,
                json_get_bool(in, "accept_callable", 1) ? 1 : 0,
                json_get_bool(in, "already_guarded", 0) ? 1 : 0) ? 1 : 0);
        } else if (!strcmp(func, "replay")) {
            out = w20_windowspro_18c6a5_replay(json_obj_get(in, "script"),
                                               json_get_bool(in, "outer_cancelled", 0) ? 1 : 0);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
