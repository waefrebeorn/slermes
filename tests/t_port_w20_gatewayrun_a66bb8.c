/* JSON-lines harness for the w20_gatewayrun_a66bb8 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_gatewayrun_a66bb8.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "client_launch_env")) {
            out = w20_gatewayrun_a66bb8_client_launch_env();
        } else if (!strcmp(func, "discover_timeout")) {
            out = json_number(w20_gatewayrun_a66bb8_discover_timeout());
        } else if (!strcmp(func, "service_session_warning")) {
            out = json_string(w20_gatewayrun_a66bb8_service_session_warning());
        } else if (!strcmp(func, "windows_service_session")) {
            out = json_bool(w20_gatewayrun_a66bb8_windows_service_session(in) ? 1 : 0);
        } else if (!strcmp(func, "failure_message")) {
            out = w20_gatewayrun_a66bb8_failure_message(json_obj_get(in, "receipt"));
        } else if (!strcmp(func, "bounded_detail")) {
            out = w20_gatewayrun_a66bb8_bounded_detail(in);
        } else if (!strcmp(func, "ticket_unavailable")) {
            out = w20_gatewayrun_a66bb8_ticket_unavailable(in);
        } else if (!strcmp(func, "attach")) {
            out = w20_gatewayrun_a66bb8_attach(in);
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
