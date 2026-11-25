/* JSON-lines harness for the w23_coordinati_5a8f30 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_coordinati_5a8f30.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "consts")) {
            out = w23_coordinati_5a8f30_consts();
        } else if (!strcmp(func, "past_replay_age")) {
            out = w23_coordinati_5a8f30_past_replay_age(json_obj_get(in, "now"),
                                                        json_obj_get(in, "completed_at"),
                                                        json_obj_get(in, "dispatched_at"));
        } else if (!strcmp(func, "json_object")) {
            out = w23_coordinati_5a8f30_json_object(json_get_str(in, "raw", NULL));
        } else if (!strcmp(func, "claim_lease")) {
            out = w23_coordinati_5a8f30_claim_lease(in);
        } else if (!strcmp(func, "replay_pending")) {
            out = w23_coordinati_5a8f30_replay_pending(in);
        } else if (!strcmp(func, "admitted_completion")) {
            out = w23_coordinati_5a8f30_admitted_completion(in);
        } else if (!strcmp(func, "sweep")) {
            out = w23_coordinati_5a8f30_sweep(in);
        } else if (!strcmp(func, "finished")) {
            out = w23_coordinati_5a8f30_finished(in);
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
