/* JSON-lines harness for the w29_adapter_1c52a4_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_1c52a4_p2.h"

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
        if (!strcmp(func, "mentions_bot")) {
            out = json_bool(w29_adapter_1c52a4_p2_mentions_bot(
                json_obj_get(in, "entities"), json_get_str(in, "text", ""),
                json_obj_get(in, "bot_ids")));
        } else if (!strcmp(func, "approval_body")) {
            out = w29_adapter_1c52a4_p2_approval_body(
                json_get_str(in, "cmd", NULL), json_get_str(in, "desc", NULL),
                json_get_bool(in, "always", 0), json_get_str(in, "header", ""),
                json_get_str(in, "reason_label", ""));
        } else if (!strcmp(func, "media_type")) {
            out = w29_adapter_1c52a4_p2_media_type(json_obj_get(in, "kinds"));
        } else if (!strcmp(func, "attachment_skip")) {
            out = json_bool(w29_adapter_1c52a4_p2_attachment_skip(
                json_get_str(in, "content_type", NULL), json_get_str(in, "content_url", NULL)));
        } else if (!strcmp(func, "document_filename")) {
            out = w29_adapter_1c52a4_p2_document_filename(
                json_get_str(in, "name", NULL), json_obj_get(in, "content"));
        } else if (!strcmp(func, "threaded_reply")) {
            out = json_bool(w29_adapter_1c52a4_p2_threaded_reply(json_get_str(in, "reply_to", NULL)));
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
