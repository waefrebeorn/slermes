#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_hostedroom_d92a93.h"
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
        if (!strcmp(func, "routed_event_id")) {
            out = w13_hostedroom_d92a93_routed_event_id(json_obj_get(in, "id"));
        } else if (!strcmp(func, "continue_count")) {
            out = w13_hostedroom_d92a93_continue_count(json_obj_get(in, "events"), json_obj_get(in, "id"));
        } else if (!strcmp(func, "routed_payload")) {
            out = w13_hostedroom_d92a93_routed_payload(json_obj_get(in, "discussion_event_id"),
                json_obj_get(in, "thread_id"), json_obj_get(in, "member_ids"));
        } else if (!strcmp(func, "one_line")) {
            out = w13_hostedroom_d92a93_one_line(json_obj_get(in, "text"));
        } else if (!strcmp(func, "parse_router_answer")) {
            out = w13_hostedroom_d92a93_parse_router_answer(json_obj_get(in, "text"),
                json_obj_get(in, "handles"));
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
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
