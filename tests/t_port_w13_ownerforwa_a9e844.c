/* t_port_w13_ownerforwa_a9e844.c — oracle harness for tui_gateway/owner_forward.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_ownerforwa_a9e844.h"

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
        if (!strcmp(func, "forwardable")) {
            out = ownerforw_forwardable(json_obj_get(in, "refusal"), json_obj_get(in, "session"),
                                        json_obj_get(in, "text"), json_obj_get(in, "params"));
        } else if (!strcmp(func, "discover_owner")) {
            out = ownerforw_discover_owner(json_obj_get(in, "state"), json_obj_get(in, "endpoint"));
        } else if (!strcmp(func, "payload_for")) {
            out = ownerforw_payload_for(json_get_str(in, "event_type", ""),
                                        json_obj_get(in, "payload"),
                                        json_obj_get(in, "contract_fields"),
                                        json_obj_get(in, "valid"));
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
