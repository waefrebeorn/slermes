/* JSON-lines harness for the w29_adapter_91f75f_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_91f75f_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "webhook_payload")) {
            out = w29_adapter_91f75f_webhook_payload(json_obj_get(in, "content"));
        } else if (!strcmp(func, "standalone_payload")) {
            out = w29_adapter_91f75f_standalone_payload(json_obj_get(in, "message"));
        } else if (!strcmp(func, "image_content")) {
            out = w29_adapter_91f75f_image_content(json_obj_get(in, "caption"),
                                                   json_obj_get(in, "image_url"));
        } else if (!strcmp(func, "no_local_upload")) {
            out = w29_adapter_91f75f_no_local_upload(json_obj_get(in, "kind"));
        } else if (!strcmp(func, "reconnect_plan")) {
            out = w29_adapter_91f75f_reconnect_plan(json_obj_get(in, "consecutive"),
                                                    json_obj_get(in, "same_type"),
                                                    json_obj_get(in, "backoff_idx"));
        } else if (!strcmp(func, "webhook_valid")) {
            out = w29_adapter_91f75f_webhook_valid(json_obj_get(in, "info"),
                                                   json_obj_get(in, "now_ms"));
        } else if (!strcmp(func, "credentials")) {
            out = w29_adapter_91f75f_credentials(json_obj_get(in, "extra"),
                                                 json_obj_get(in, "env_id"),
                                                 json_obj_get(in, "env_secret"));
        } else if (!strcmp(func, "is_connected")) {
            out = w29_adapter_91f75f_is_connected(json_obj_get(in, "extra"),
                                                  json_obj_get(in, "env_id"),
                                                  json_obj_get(in, "env_secret"));
        } else if (!strcmp(func, "nested_allowed_users")) {
            out = w29_adapter_91f75f_nested_allowed_users(json_obj_get(in, "yaml_cfg"),
                                                          json_obj_get(in, "dingtalk_cfg"));
        } else if (!strcmp(func, "sdk_log_key")) {
            out = w29_adapter_91f75f_sdk_log_key(json_obj_get(in, "level"),
                                                 json_obj_get(in, "msg"),
                                                 json_obj_get(in, "exc_type"));
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
