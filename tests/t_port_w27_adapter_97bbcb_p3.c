/* JSON-lines harness for the w27_adapter_97bbcb_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_97bbcb_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "chunk_text")) {
            out = w27_gchat_p3_chunk_text(in);
        } else if (!strcmp(func, "resolve_thread_id")) {
            out = w27_gchat_p3_resolve_thread_id(in);
        } else if (!strcmp(func, "patch_shape")) {
            out = w27_gchat_p3_patch_shape(in);
        } else if (!strcmp(func, "send_card_shape")) {
            out = w27_gchat_p3_send_card_shape(in);
        } else if (!strcmp(func, "attachment_body")) {
            out = w27_gchat_p3_attachment_body(in);
        } else if (!strcmp(func, "media_filename")) {
            out = w27_gchat_p3_media_filename(in);
        } else if (!strcmp(func, "upload_basename")) {
            out = w27_gchat_p3_upload_basename(in);
        } else if (!strcmp(func, "note_rate_limit")) {
            out = w27_gchat_p3_note_rate_limit(in);
        } else if (!strcmp(func, "typing_send_action")) {
            out = w27_gchat_p3_typing_send_action(in);
        } else if (!strcmp(func, "typing_stop_action")) {
            out = w27_gchat_p3_typing_stop_action(in);
        } else if (!strcmp(func, "typing_consume_action")) {
            out = w27_gchat_p3_typing_consume_action(in);
        } else if (!strcmp(func, "chat_info_shape")) {
            out = w27_gchat_p3_chat_info_shape(in);
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
