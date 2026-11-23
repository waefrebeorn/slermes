/* t_port_w8_clientlifec_20bd72.c — harness for agent/client_lifecycle.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_clientlifec_20bd72.h"

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
        if (!strcmp(func, "valid_pair")) {
            out = json_bool(w8_clientlifec_20bd72_valid_credential_pair(
                json_obj_get(in, "api_key"), json_obj_get(in, "base_url")) ? true : false);
        } else if (!strcmp(func, "reset_slot")) {
            out = w8_clientlifec_20bd72_reset_slot(json_obj_get(in, "cache"),
                       json_get_bool(in, "in_use", false) ? 1 : 0);
        } else if (!strcmp(func, "image_parts")) {
            out = json_bool(w8_clientlifec_20bd72_api_kwargs_have_image_parts(
                json_obj_get(in, "api_kwargs")) ? true : false);
        } else if (!strcmp(func, "client_closed")) {
            out = json_bool(w8_clientlifec_20bd72_is_openai_client_closed(
                json_obj_get(in, "client")) ? true : false);
        } else if (!strcmp(func, "should_adopt")) {
            out = json_bool(w8_clientlifec_20bd72_should_adopt_env_credentials(
                json_obj_get(in, "args")) ? true : false);
        } else if (!strcmp(func, "log_context")) {
            out = w8_clientlifec_20bd72_anthropic_log_context(json_obj_get(in, "agent"));
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
