/* JSON-lines harness for the w28_messaging_ede80b_p2 gate (Telegram onboarding).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_messaging_ede80b_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize_user_id")) {
            out = w28_messaging_ede80b_p2_normalize_user_id(json_obj_get(in, "value"));
        } else if (!strcmp(func, "normalize_allowed_ids")) {
            out = w28_messaging_ede80b_p2_normalize_allowed_ids(json_obj_get(in, "ids"));
        } else if (!strcmp(func, "parse_expiry_ts")) {
            out = w28_messaging_ede80b_p2_parse_expiry_ts(json_obj_get(in, "value"));
        } else if (!strcmp(func, "ready_payload")) {
            out = w28_messaging_ede80b_p2_ready_payload(json_obj_get(in, "record"));
        } else if (!strcmp(func, "start_payload")) {
            out = w28_messaging_ede80b_p2_start_payload(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "status_payload")) {
            out = w28_messaging_ede80b_p2_status_payload(json_obj_get(in, "record"),
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
