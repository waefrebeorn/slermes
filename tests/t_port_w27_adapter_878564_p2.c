/* JSON-lines harness for the w27_adapter_878564_p2 gate (photon adapter:
 * inbound content predicates and labels).  One request object per stdin line;
 * one response line carrying the computed "out" plus every input key echoed. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_878564_p2.h"

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
        if (!strcmp(func, "is_voice_payload")) {
            out = json_bool(w27_adapter_878564_p2_is_voice_payload(json_obj_get(in, "payload")) != 0);
        } else if (!strcmp(func, "cached_media_type")) {
            out = json_string(w27_adapter_878564_p2_cached_media_type(
                json_obj_get(in, "payload"), json_get_str(in, "mime", ""),
                json_get_bool(in, "is_voice", false)));
        } else if (!strcmp(func, "attachment_message_type")) {
            out = json_string(w27_adapter_878564_p2_attachment_message_type(
                json_get_str(in, "mime", "")));
        } else if (!strcmp(func, "group_item_contents")) {
            out = w27_adapter_878564_p2_group_item_contents(json_obj_get(in, "content"));
        } else if (!strcmp(func, "richlink_url_from_content")) {
            char *s = w27_adapter_878564_p2_richlink_url_from_content(json_obj_get(in, "content"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "is_richlink_preview_attachment")) {
            out = json_bool(w27_adapter_878564_p2_is_richlink_preview_attachment(
                                json_obj_get(in, "payload")) != 0);
        } else if (!strcmp(func, "richlink_preview_label")) {
            char *s = w27_adapter_878564_p2_richlink_preview_label(json_obj_get(in, "content"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "is_richlink_preview_content")) {
            out = json_bool(w27_adapter_878564_p2_is_richlink_preview_content(
                                json_obj_get(in, "content")) != 0);
        } else if (!strcmp(func, "reply_inner")) {
            out = w27_adapter_878564_p2_reply_inner(json_obj_get(in, "content"));
        } else if (!strcmp(func, "carries_voice")) {
            out = json_bool(w27_adapter_878564_p2_carries_voice(json_obj_get(in, "content")) != 0);
        } else if (!strcmp(func, "mention_gate_text")) {
            char *s = w27_adapter_878564_p2_mention_gate_text(json_obj_get(in, "content"));
            out = s ? json_string(s) : json_null();
            free(s);
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
