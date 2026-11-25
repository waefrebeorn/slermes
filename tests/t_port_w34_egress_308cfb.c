/* JSON-lines harness for the w34_egress_308cfb gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_egress_308cfb.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_egress_decline")) {
            out = json_bool(w34_egress_308cfb_is_egress_decline(json_obj_get(in, "result")));
        } else if (!strcmp(func, "declined_send")) {
            out = json_bool(w34_egress_308cfb_declined_send(json_obj_get(in, "result")));
        } else if (!strcmp(func, "decline_error")) {
            out = w34_egress_308cfb_decline_error(json_obj_get(in, "result"));
        } else if (!strcmp(func, "decline_log_line")) {
            out = w34_egress_308cfb_decline_log_line(json_obj_get(in, "op"),
                                                    json_obj_get(in, "chat_id"),
                                                    json_obj_get(in, "result"));
        } else if (!strcmp(func, "is_missing_gateway_relay")) {
            out = json_bool(w34_egress_308cfb_is_missing_gateway_relay(
                json_get_bool(in, "module_not_found", 0),
                json_get_str(in, "name", NULL)));
        } else if (!strcmp(func, "has_live_native_adapter")) {
            out = w34_egress_308cfb_has_live_native_adapter(
                json_get_bool(in, "runner_present", 0),
                json_get_bool(in, "adapter_present", 0),
                json_get_bool(in, "config_error", 0),
                json_obj_get(in, "native_config"));
        } else if (!strcmp(func, "relay_routed_platform")) {
            out = w34_egress_308cfb_relay_routed_platform(in);
        } else if (!strcmp(func, "session_ids")) {
            out = w34_egress_308cfb_session_ids(json_obj_get(in, "entries"));
        } else if (!strcmp(func, "attested_relay_targets")) {
            out = w34_egress_308cfb_attested_relay_targets(in);
        } else if (!strcmp(func, "is_unresolved_handle")) {
            out = json_bool(w34_egress_308cfb_is_unresolved_handle(in));
        } else if (!strcmp(func, "authorize_relay_target")) {
            out = w34_egress_308cfb_authorize_relay_target(in);
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
