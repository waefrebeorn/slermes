/* JSON-lines harness for the w26_telegramba_f62536 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramba_f62536.h"

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
        if (!strcmp(func, "pad_inline_flags")) {
            out = w26_telegramba_f62536_pad_inline_flags(json_obj_get(in, "media_text_inlined"),
                                                         json_obj_get(in, "media_urls"));
        } else if (!strcmp(func, "command_first")) {
            out = w26_telegramba_f62536_command_first(json_obj_get(in, "captions"),
                                                      json_obj_get(in, "incoming"));
        } else if (!strcmp(func, "merged_text")) {
            out = w26_telegramba_f62536_merged_text(json_obj_get(in, "text"),
                                                    json_obj_get(in, "incoming"),
                                                    json_obj_get(in, "notes"));
        } else if (!strcmp(func, "caption_is_command")) {
            const json_t *c = json_obj_get(in, "caption");
            out = json_bool(w26_telegramba_f62536_caption_is_command(
                c && c->type == JSON_STRING ? c->str_val : NULL));
        } else if (!strcmp(func, "constants")) {
            out = w26_telegramba_f62536_constants();
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
