/* JSON-lines harness for the w33_standalone_9f791b gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_standalone_9f791b.h"

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
        if (!strcmp(func, "caption_split")) {
            out = w33_standalone_9f791b_caption_split(in);
        } else if (!strcmp(func, "media_kind")) {
            out = w33_standalone_9f791b_media_kind(in);
        } else if (!strcmp(func, "album_shape")) {
            out = w33_standalone_9f791b_album_shape(in);
        } else if (!strcmp(func, "thread_kwargs")) {
            out = w33_standalone_9f791b_thread_kwargs(in);
        } else if (!strcmp(func, "thread_is_name")) {
            out = w33_standalone_9f791b_thread_is_name(in);
        } else if (!strcmp(func, "thread_not_found")) {
            out = w33_standalone_9f791b_thread_not_found(in);
        } else if (!strcmp(func, "html_tag_present")) {
            out = w33_standalone_9f791b_html_tag_present(in);
        } else if (!strcmp(func, "escape_markup")) {
            out = w33_standalone_9f791b_escape_markup(in);
        } else if (!strcmp(func, "fallback_ips")) {
            out = w33_standalone_9f791b_fallback_ips(in);
        } else if (!strcmp(func, "resolve_token")) {
            out = w33_standalone_9f791b_resolve_token(in);
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
