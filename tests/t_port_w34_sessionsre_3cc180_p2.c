/* JSON-lines harness for the w34_sessionsre_3cc180_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_sessionsre_3cc180_p2.h"

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
        if (!strcmp(func, "voice_modes")) {
            out = w34_sessionsre_3cc180_p2_voice_modes(in);
        } else if (!strcmp(func, "sessions_json")) {
            out = w34_sessionsre_3cc180_p2_sessions_json(in);
        } else if (!strcmp(func, "parents_first")) {
            out = w34_sessionsre_3cc180_p2_parents_first(in);
        } else if (!strcmp(func, "rekey_voice")) {
            out = w34_sessionsre_3cc180_p2_rekey_voice(in);
        } else if (!strcmp(func, "drop_entry")) {
            out = w34_sessionsre_3cc180_p2_drop_entry(in);
        } else if (!strcmp(func, "move_batch")) {
            out = w34_sessionsre_3cc180_p2_move_batch(in);
        } else if (!strcmp(func, "apply")) {
            out = w34_sessionsre_3cc180_p2_apply(in);
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
