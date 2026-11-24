/* JSON-lines harness for the w27_adapter_878564_p4 gate (photon adapter: ids,
 * sidecar error typing, retry classification, constants, timestamps).  One
 * request object per stdin line; one response line carrying "out" plus every
 * input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_878564_p4.h"

static void emit_val(json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static void emit_key(const char *key, json_t *v) {
    printf("\"%s\":", key);
    emit_val(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize_chat_key")) {
            char *s = w27_adapter_878564_p4_normalize_chat_key(json_get_str(in, "chat_id", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "sent_text_key")) {
            char *s = w27_adapter_878564_p4_sent_text_key(json_get_str(in, "chat_id", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "is_dm_guid")) {
            out = json_bool(w27_adapter_878564_p4_is_dm_guid(json_get_str(in, "text", "")) != 0);
        } else if (!strcmp(func, "sidecar_error_from_response")) {
            out = w27_adapter_878564_p4_sidecar_error_from_response(
                json_get_str(in, "path", ""), json_get_num(in, "status_code", 0),
                json_get_str(in, "text", ""), json_obj_get(in, "data"));
        } else if (!strcmp(func, "retryable_error")) {
            out = json_bool(w27_adapter_878564_p4_retryable_error(
                                json_get_str(in, "error", ""),
                                json_get_bool(in, "base_retryable", false)) != 0);
        } else if (!strcmp(func, "is_permanent_sidecar_failure")) {
            out = json_bool(w27_adapter_878564_p4_is_permanent_sidecar_failure(
                                json_obj_get(in, "result")) != 0);
        } else if (!strcmp(func, "send_retry_is_final")) {
            out = json_bool(w27_adapter_878564_p4_send_retry_is_final(
                                json_obj_get(in, "result")) != 0);
        } else if (!strcmp(func, "constants")) {
            out = w27_adapter_878564_p4_constants();
        } else if (!strcmp(func, "parse_timestamp")) {
            out = w27_adapter_878564_p4_parse_timestamp(json_get_str(in, "ts", NULL),
                                                        json_get_str(in, "now", NULL));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",\"out\":", stdout);
        emit_val(out);
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
