/* JSON-lines harness for the w24_onboard_d18acb gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_onboard_d18acb.h"

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
        if (!strcmp(func, "url")) {
            out = w24_onboard_d18acb_url(json_obj_get(in, "task_id"));
        } else if (!strcmp(func, "poll_result")) {
            out = w24_onboard_d18acb_poll_result(json_obj_get(in, "data"));
        } else if (!strcmp(func, "portal_error")) {
            out = w24_onboard_d18acb_portal_error(json_obj_get(in, "data"),
                                                  json_obj_get(in, "fail_msg"));
        } else if (!strcmp(func, "create_result")) {
            out = w24_onboard_d18acb_create_result(json_obj_get(in, "data"));
        } else if (!strcmp(func, "expiry")) {
            out = w24_onboard_d18acb_expiry(json_obj_get(in, "refresh_count"));
        } else if (!strcmp(func, "status_name")) {
            out = w24_onboard_d18acb_status_name(json_obj_get(in, "status"));
        } else if (!strcmp(func, "constants")) {
            out = w24_onboard_d18acb_constants();
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
