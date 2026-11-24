/* JSON-lines harness for the w20_prompts_c0fcfd gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 * The port .c is included by path so the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_prompts_c0fcfd.h"
#include "../src/agent/pet/generate/prompts.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "style_hint")) {
            out = json_string(w20_prompts_c0fcfd_style_hint(str_or(in, "style", NULL)));
        } else if (!strcmp(func, "state_action")) {
            out = json_string(w20_prompts_c0fcfd_state_action(str_or(in, "state", NULL)));
        } else if (!strcmp(func, "spacing_spec")) {
            out = w20_prompts_c0fcfd_spacing_spec(json_get_num(in, "frame_count", 0.0));
        } else if (!strcmp(func, "base_variations")) {
            out = w20_prompts_c0fcfd_base_variations();
        } else if (!strcmp(func, "base_prompt")) {
            char *s = w20_prompts_c0fcfd_build_base_prompt(str_or(in, "concept", NULL),
                                                           str_or(in, "style", NULL),
                                                           str_or(in, "variation", NULL));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "row_prompt")) {
            char *s = w20_prompts_c0fcfd_build_row_prompt(str_or(in, "state", NULL),
                                                          json_get_num(in, "frame_count", 0.0),
                                                          str_or(in, "concept", NULL),
                                                          str_or(in, "style", NULL));
            out = json_string(s ? s : "");
            free(s);
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
