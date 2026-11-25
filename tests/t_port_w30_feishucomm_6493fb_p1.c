/* JSON-lines harness for the w30_feishucomm_6493fb_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_feishucomm_6493fb_p1.h"

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
        if (!strcmp(func, "parse_event")) {
            out = w30_feishucomm_6493fb_p1_parse_event(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "meta")) {
            out = w30_feishucomm_6493fb_p1_meta_result(in);
        } else if (!strcmp(func, "comment")) {
            out = w30_feishucomm_6493fb_p1_comment_result(in);
        } else if (!strcmp(func, "pages")) {
            out = w30_feishucomm_6493fb_p1_pages_result(in);
        } else if (!strcmp(func, "reaction")) {
            out = w30_feishucomm_6493fb_p1_reaction_result(in);
        } else if (!strcmp(func, "reply")) {
            out = w30_feishucomm_6493fb_p1_reply_result(in);
        } else if (!strcmp(func, "whole_comment")) {
            out = w30_feishucomm_6493fb_p1_whole_comment_result(in);
        } else if (!strcmp(func, "deliver")) {
            out = w30_feishucomm_6493fb_p1_deliver_result(in);
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
