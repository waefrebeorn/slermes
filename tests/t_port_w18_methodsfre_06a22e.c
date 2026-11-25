#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_methodsfre_06a22e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        const char *rid = json_get_str(in, "rid", "");
        json_t *a = json_obj_get(in, "in");
        if (!strcmp(func, "ack_notice")) {
            json_t *gv = json_obj_get(in, "guest");
            int guest = 0;
            if (gv && gv->type == JSON_BOOL) guest = gv->bool_val ? 1 : 0;
            else if (gv && gv->type == JSON_NUMBER) guest = gv->num_val != 0.0;
            out = w18_methodsfre_06a22e_ack_notice(rid, guest);
        }
        else if (!strcmp(func, "claim_nudge"))
            out = w18_methodsfre_06a22e_claim_nudge(rid, a);
        else if (!strcmp(func, "challenge_result"))
            out = w18_methodsfre_06a22e_challenge_result(rid, a);
        else out = json_string("UNKNOWN_FUNC");
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
