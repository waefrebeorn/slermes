#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_toolsearch_6ec2b5.h"

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
        if (!strcmp(func, "short_desc")) {
            out = w12_toolsearch_6ec2b5_short_desc(json_obj_get(in,"description"),
                    (long long)json_get_num(in,"max_chars",80));
        } else if (!strcmp(func, "listing_group_label")) {
            out = w12_toolsearch_6ec2b5_listing_group_label(json_obj_get(in,"source_name"));
        } else if (!strcmp(func, "fn")) {
            out = w12_toolsearch_6ec2b5_fn(json_obj_get(in,"td"));
        } else if (!strcmp(func, "required_term_coverage")) {
            out = w12_toolsearch_6ec2b5_required_term_coverage(json_obj_get(in,"n"));
        } else if (!strcmp(func, "entry_search_text")) {
            out = w12_toolsearch_6ec2b5_entry_search_text(json_obj_get(in,"td"),
                    json_obj_get(in,"source_label"));
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
