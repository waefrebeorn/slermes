/* JSON-lines harness for the w21_webserveri_184aef gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * an absent turn/pending probe reads as None, and an absent count as 0.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_webserveri_184aef.h"
#include "hermes_cli/web_server_idle_proof.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "idle_proof")) {
            out = w21_webserveri_184aef_idle_proof(json_obj_get(in, "turn"),
                                                   json_obj_get(in, "pending"));
        } else if (!strcmp(func, "pending_human_input")) {
            json_t *un = json_obj_get(in, "unavailable");
            out = w21_webserveri_184aef_pending_human_input(json_obj_get(in, "server_requests"),
                                                            json_obj_get(in, "approvals"),
                                                            un && wsp184_truthy(un));
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
