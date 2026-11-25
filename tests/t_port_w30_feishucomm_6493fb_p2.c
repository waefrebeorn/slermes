/* JSON-lines harness for the w30_feishucomm_6493fb_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_feishucomm_6493fb_p2.h"

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
        if (!strcmp(func, "sanitize")) {
            out = w30_feishucomm_6493fb_p2_sanitize(in);
        } else if (!strcmp(func, "chunk")) {
            out = w30_feishucomm_6493fb_p2_chunk(in);
        } else if (!strcmp(func, "extract_text")) {
            out = w30_feishucomm_6493fb_p2_extract_text(in);
        } else if (!strcmp(func, "reply_user")) {
            out = w30_feishucomm_6493fb_p2_reply_user(in);
        } else if (!strcmp(func, "reply_list")) {
            out = w30_feishucomm_6493fb_p2_reply_list(in);
        } else if (!strcmp(func, "extract_links")) {
            out = w30_feishucomm_6493fb_p2_extract_links(in);
        } else if (!strcmp(func, "format_docs")) {
            out = w30_feishucomm_6493fb_p2_format_docs(in);
        } else if (!strcmp(func, "truncate")) {
            out = w30_feishucomm_6493fb_p2_truncate(in);
        } else if (!strcmp(func, "select")) {
            out = w30_feishucomm_6493fb_p2_select(in);
        } else if (!strcmp(func, "timeline")) {
            out = w30_feishucomm_6493fb_p2_timeline(in);
        } else if (!strcmp(func, "build_local")) {
            out = w30_feishucomm_6493fb_p2_build_local(in);
        } else if (!strcmp(func, "build_whole")) {
            out = w30_feishucomm_6493fb_p2_build_whole(in);
        } else if (!strcmp(func, "whole_prompt")) {
            out = w30_feishucomm_6493fb_p2_whole_prompt(in);
        } else if (!strcmp(func, "local_prompt")) {
            out = w30_feishucomm_6493fb_p2_local_prompt(in);
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
