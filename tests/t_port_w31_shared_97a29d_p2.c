/* JSON-lines harness for the w31_shared_97a29d_p2 gate (upstream
 * gateway/platforms/_shared.py delivery + payload shaping).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_shared_97a29d_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "delivery_state")) {
            out = w31_shared_97a29d_p2_delivery_state(json_obj_get(in, "result"));
        } else if (!strcmp(func, "delivered_message_ids")) {
            out = w31_shared_97a29d_p2_delivered_message_ids(json_obj_get(in, "result"));
        } else if (!strcmp(func, "outcome_failure")) {
            out = w31_shared_97a29d_p2_outcome_failure(json_obj_get(in, "error"),
                                                       json_obj_get(in, "message_ids"),
                                                       json_obj_get(in, "has_landed"),
                                                       json_obj_get(in, "is_ambiguous"),
                                                       json_obj_get(in, "never_sent"));
        } else if (!strcmp(func, "split_fields")) {
            out = w31_shared_97a29d_p2_split_fields(json_obj_get(in, "message_ids"));
        } else if (!strcmp(func, "yaml_kind_applies")) {
            out = w31_shared_97a29d_p2_yaml_kind_applies(json_obj_get(in, "cfg"),
                                                         json_get_str(in, "key", ""),
                                                         json_get_str(in, "kind", ""));
        } else if (!strcmp(func, "yaml_bridge_seed")) {
            out = w31_shared_97a29d_p2_yaml_bridge_seed(json_obj_get(in, "cfg"),
                                                        json_obj_get(in, "spec"));
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
        json_free(in);
    }
    return 0;
}
