#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w10_slashfuzzy_f16515.h"
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
        json_t *out = NULL;
        if (!strcmp(func, "tokenize")) {
            out = w10_slashfuzzy_f16515_tokenize_search_text(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "normalize")) {
            char *v = w10_slashfuzzy_f16515_normalize_slash_search_query(json_get_str(in, "query", ""));
            out = json_string(v ? v : ""); free(v);
        } else if (!strcmp(func, "score")) {
            double s = w10_slashfuzzy_f16515_score_slash_completion_item(json_obj_get(in, "item"),
                                                                         json_get_str(in, "query", ""));
            out = isinf(s) ? json_string("inf") : json_number(s);
        } else if (!strcmp(func, "rank")) {
            out = w10_slashfuzzy_f16515_fuzzy_rank_slash_items(json_obj_get(in, "items"),
                       json_obj_get(in, "catalog"), json_get_str(in, "query", ""));
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
