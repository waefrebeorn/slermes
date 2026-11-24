/* t_port_w16_slashcomma_e06e76.c — harness for src/gateway/slash_commands_model.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_slashcomma_e06e76.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "fast_selections_table"))
            out = w16_slashcomma_e06e76_fast_selections_table();
        else if (!strcmp(func, "fast_selection"))
            out = w16_slashcomma_e06e76_fast_selection(json_get_str(in, "arg", NULL));
        else if (!strcmp(func, "reasoning_toggles_table"))
            out = w16_slashcomma_e06e76_reasoning_toggles_table();
        else if (!strcmp(func, "reasoning_toggle"))
            out = w16_slashcomma_e06e76_reasoning_toggle(json_get_str(in, "arg", NULL));
        else if (!strcmp(func, "text_listing_models"))
            out = json_number((double)w16_slashcomma_e06e76_text_listing_models());
        else
            out = json_string("UNKNOWN_FUNC");
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
