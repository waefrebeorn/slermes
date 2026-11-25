/* JSON-lines harness for the w29_adapter_d36d4e_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_d36d4e_p3.h"

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
        json_t *in, *out, *fnode;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "with_target")) {
            out = w29_adapter_d36d4e_p3_with_target(json_obj_get(in, "chat_id"),
                                                    json_obj_get(in, "account"));
        } else if (!strcmp(func, "validate_send_result")) {
            out = w29_adapter_d36d4e_p3_validate_send_result(json_obj_get(in, "result"));
        } else if (!strcmp(func, "utf16_offsets")) {
            out = w29_adapter_d36d4e_p3_utf16_offsets(json_obj_get(in, "text"));
        } else if (!strcmp(func, "styles_for_chunk")) {
            out = w29_adapter_d36d4e_p3_styles_for_chunk(json_obj_get(in, "styles"),
                                                         json_obj_get(in, "start"),
                                                         json_obj_get(in, "end"));
        } else if (!strcmp(func, "split_signal_message")) {
            out = w29_adapter_d36d4e_p3_split_signal_message(json_obj_get(in, "text"),
                                                             json_obj_get(in, "styles"),
                                                             json_obj_get(in, "max_length"));
        } else if (!strcmp(func, "track_sent_timestamp")) {
            out = w29_adapter_d36d4e_p3_track_sent_timestamp(json_obj_get(in, "result"),
                                                             json_obj_get(in, "recent"),
                                                             json_obj_get(in, "sent_cache"),
                                                             json_obj_get(in, "now"),
                                                             json_obj_get(in, "ttl"),
                                                             json_obj_get(in, "max_recent"),
                                                             json_obj_get(in, "max_sent"));
        } else if (!strcmp(func, "typing_skip_seconds")) {
            out = w29_adapter_d36d4e_p3_typing_skip_seconds(json_obj_get(in, "prior_fails"));
        } else if (!strcmp(func, "extract_reaction_target")) {
            out = w29_adapter_d36d4e_p3_extract_reaction_target(json_obj_get(in, "raw_message"));
        } else if (!strcmp(func, "reaction_params")) {
            out = w29_adapter_d36d4e_p3_reaction_params(json_obj_get(in, "account"),
                                                        json_obj_get(in, "chat_id"),
                                                        json_obj_get(in, "emoji"),
                                                        json_obj_get(in, "author"),
                                                        json_obj_get(in, "timestamp"),
                                                        json_obj_get(in, "remove"));
        } else if (!strcmp(func, "image_error_text")) {
            out = w29_adapter_d36d4e_p3_image_error_text(json_obj_get(in, "reason"),
                                                         json_obj_get(in, "detail"));
        } else if (!strcmp(func, "chat_info")) {
            out = w29_adapter_d36d4e_p3_chat_info(json_obj_get(in, "chat_id"),
                                                  json_obj_get(in, "contact"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        fnode = json_string(func);
        emit_key("func", fnode);
        json_free(fnode);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            json_t *v;
            if (!k || !strcmp(k, "func")) continue;
            v = json_copy(json_object_get_at(in, i));
            fputs(",", stdout);
            emit_key(k, v);
            json_free(v);
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
