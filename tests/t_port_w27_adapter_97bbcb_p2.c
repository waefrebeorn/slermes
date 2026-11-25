/* JSON-lines harness for the w27_adapter_97bbcb_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_97bbcb_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_google_owned_host")) {
            out = w27_gchat_p2_is_google_owned_host(in);
        } else if (!strcmp(func, "redact_sensitive")) {
            out = w27_gchat_p2_redact_sensitive(in);
        } else if (!strcmp(func, "retry_class")) {
            out = w27_gchat_p2_retry_class(in);
        } else if (!strcmp(func, "validate_config")) {
            out = w27_gchat_p2_validate_config(in);
        } else if (!strcmp(func, "plugin_validate_config")) {
            out = w27_gchat_p2_plugin_validate_config(in);
        } else if (!strcmp(func, "is_connected")) {
            out = w27_gchat_p2_is_connected(in);
        } else if (!strcmp(func, "space_id_ok")) {
            out = w27_gchat_p2_space_id_ok(in);
        } else if (!strcmp(func, "thread_id_ok")) {
            out = w27_gchat_p2_thread_id_ok(in);
        } else if (!strcmp(func, "verify_http_event")) {
            out = w27_gchat_p2_verify_http_event(in);
        } else if (!strcmp(func, "int_setting")) {
            out = w27_gchat_p2_int_setting(in);
        } else if (!strcmp(func, "str_setting")) {
            out = w27_gchat_p2_str_setting(in);
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
