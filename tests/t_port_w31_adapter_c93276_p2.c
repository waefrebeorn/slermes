/* JSON-lines harness for the w31_adapter_c93276_p2 gate (SimpleX outbound).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_adapter_c93276_p2.h"

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
        if (!strcmp(func, "send_cmd")) {
            out = w31_adapter_c93276_p2_send_cmd(json_get_str(in, "chat_id", ""),
                                                 json_obj_get(in, "items"));
        } else if (!strcmp(func, "split_media")) {
            out = w31_adapter_c93276_p2_split_media(json_get_str(in, "content", ""));
        } else if (!strcmp(func, "is_voice_ext")) {
            out = w31_adapter_c93276_p2_is_voice_ext(json_get_str(in, "ext", ""));
        } else if (!strcmp(func, "send_item")) {
            out = w31_adapter_c93276_p2_send_item(json_get_str(in, "kind", "document"),
                                                  json_get_str(in, "path", ""),
                                                  json_obj_get(in, "caption"),
                                                  json_obj_get(in, "duration"),
                                                  json_get_str(in, "thumb", ""));
        } else if (!strcmp(func, "corr_id")) {
            out = w31_adapter_c93276_p2_corr_id(json_obj_get(in, "counter"),
                                                json_obj_get(in, "now_ms"));
        } else if (!strcmp(func, "retry_math")) {
            out = w31_adapter_c93276_p2_retry_math(json_obj_get(in, "backoff"),
                                                   json_obj_get(in, "rnd"));
        } else if (!strcmp(func, "health_stale")) {
            out = w31_adapter_c93276_p2_health_stale(json_obj_get(in, "elapsed"));
        } else if (!strcmp(func, "init_config")) {
            out = w31_adapter_c93276_p2_init_config(json_obj_get(in, "env"),
                                                    json_obj_get(in, "extra"));
        } else if (!strcmp(func, "config_gate")) {
            out = w31_adapter_c93276_p2_config_gate(json_obj_get(in, "env"),
                                                    json_obj_get(in, "extra"),
                                                    json_obj_get(in, "websockets"));
        } else if (!strcmp(func, "env_enablement")) {
            out = w31_adapter_c93276_p2_env_enablement(json_obj_get(in, "env"));
        } else if (!strcmp(func, "get_chat_info")) {
            out = w31_adapter_c93276_p2_get_chat_info(json_get_str(in, "chat_id", ""));
        } else if (!strcmp(func, "list_channels")) {
            out = w31_adapter_c93276_p2_list_channels(json_obj_get(in, "contacts_resp"),
                                                      json_obj_get(in, "groups_resp"));
        } else if (!strcmp(func, "prepare_image")) {
            out = w31_adapter_c93276_p2_prepare_image(json_get_str(in, "file_path", ""));
        } else if (!strcmp(func, "text_batch_key")) {
            out = w31_adapter_c93276_p2_text_batch_key(json_get_str(in, "platform", "simplex"),
                                                       json_get_str(in, "chat_id", ""));
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
