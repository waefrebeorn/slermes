#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_redactbrow_551284.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *code = json_get_str(in, "code", "");
        json_t *out = NULL;
        if (!strcmp(func, "literal_end")) {
            out = w13_redactbrow_551284_literal_end(code, (int)json_get_num(in, "start", 0));
        } else if (!strcmp(func, "skip_comment")) {
            out = json_number((double)w13_redactbrow_551284_skip_comment(code,
                      (int)json_get_num(in, "i", 0)));
        } else if (!strcmp(func, "literal_mask")) {
            out = w13_redactbrow_551284_literal_mask(code, (int)json_get_num(in, "start", 0));
        } else if (!strcmp(func, "call_end")) {
            out = json_number((double)w13_redactbrow_551284_call_end(code,
                      (int)json_get_num(in, "i", 0)));
        } else if (!strcmp(func, "literal_regions")) {
            out = w13_redactbrow_551284_literal_regions(code,
                      (int)json_get_num(in, "start", 0), (int)json_get_num(in, "end", 0));
        } else if (!strcmp(func, "unparsed_masks")) {
            out = w13_redactbrow_551284_unparsed_masks(code);
        } else if (!strcmp(func, "mask_unparsed")) {
            out = w13_redactbrow_551284_mask_unparsed(code);
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
