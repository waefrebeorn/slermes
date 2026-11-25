#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_sessionlif_2e5c9b.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "lease_metadata"))
            out = w18_sessionlif_2e5c9b_lease_metadata(json_get_str(a, "live_session_id", ""));
        else if (!strcmp(func, "is_gateway_owned_source"))
            out = w18_sessionlif_2e5c9b_is_gateway_owned_source(json_get_str(a, "source", ""));
        else if (!strcmp(func, "lifecycle_own_sid"))
            out = w18_sessionlif_2e5c9b_lifecycle_own_sid(
                a ? json_obj_get(a, "session") : NULL, json_get_str(a, "sid_hint", ""));
        else if (!strcmp(func, "session_holds_transcript_claim"))
            out = w18_sessionlif_2e5c9b_session_holds_transcript_claim(
                a ? json_obj_get(a, "session") : NULL, json_get_num(a, "now", 0.0));
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
