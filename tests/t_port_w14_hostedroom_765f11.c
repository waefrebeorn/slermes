#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_hostedroom_765f11.h"
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "identifier")) {
            out = w14_hostedroom_765f11_identifier(json_obj_get(in,"value"), json_obj_get(in,"label"),
                json_obj_get(in,"max_chars"), json_obj_get(in,"pattern_mode"), json_obj_get(in,"invalid"));
        } else if (!strcmp(func, "bounded_int")) {
            out = w14_hostedroom_765f11_bounded_int(json_obj_get(in,"value"), json_obj_get(in,"message"),
                json_obj_get(in,"low"), json_obj_get(in,"high"));
        } else if (!strcmp(func, "text")) {
            out = w14_hostedroom_765f11_text(json_obj_get(in,"value"), json_obj_get(in,"label"),
                json_obj_get(in,"max_bytes"), json_obj_get(in,"strip"));
        } else if (!strcmp(func, "validate_context")) {
            out = w14_hostedroom_765f11_validate_context(json_obj_get(in,"purpose"), json_obj_get(in,"notes"),
                json_obj_get(in,"routing"), json_obj_get(in,"lead_member_id"));
        } else if (!strcmp(func, "context_from_payload")) {
            out = w14_hostedroom_765f11_context_from_payload(json_obj_get(in,"payload"));
        } else if (!strcmp(func, "retry_fields")) {
            out = w14_hostedroom_765f11_retry_fields(json_obj_get(in,"context"), json_obj_get(in,"original"));
        } else if (!strcmp(func, "retained_fields")) {
            out = w14_hostedroom_765f11_retained_fields(json_obj_get(in,"table"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
