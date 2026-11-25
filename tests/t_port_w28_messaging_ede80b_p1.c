/* JSON-lines harness for the w28_messaging_ede80b_p1 gate (WhatsApp onboarding).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_messaging_ede80b_p1.h"

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
        if (!strcmp(func, "normalize_mode")) {
            out = w28_messaging_ede80b_p1_normalize_mode(json_obj_get(in, "value"));
        } else if (!strcmp(func, "normalize_allowed_users")) {
            out = w28_messaging_ede80b_p1_normalize_allowed_users(json_obj_get(in, "value"));
        } else if (!strcmp(func, "phone_from_identifier")) {
            out = w28_messaging_ede80b_p1_phone_from_identifier(json_obj_get(in, "value"));
        } else if (!strcmp(func, "first_str")) {
            out = w28_messaging_ede80b_p1_first_str(json_obj_get(in, "candidate"),
                                                    json_obj_get(in, "keys"));
        } else if (!strcmp(func, "linked_account")) {
            out = w28_messaging_ede80b_p1_linked_account(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "apply_pairing_event")) {
            out = w28_messaging_ede80b_p1_apply_pairing_event(json_obj_get(in, "record"),
                                                              json_obj_get(in, "payload"));
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
