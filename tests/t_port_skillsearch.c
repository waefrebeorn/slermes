#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_skillsearch.h"
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
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
        if (!strcmp(func, "provider_filter")) {
            out = skillsearch_provider_filter(json_get_str(in, "source_filter", ""));
        } else if (!strcmp(func, "select_active")) {
            out = skillsearch_select_active(json_obj_get(in, "sources"), json_get_str(in, "source_filter", "all"));
        } else if (!strcmp(func, "index_miss_fallback")) {
            out = skillsearch_index_miss_fallback(json_obj_get(in, "all_ids"), json_obj_get(in, "active_ids"),
                                                  json_get_str(in, "query", ""), json_obj_get(in, "source_counts"),
                                                  json_get_str(in, "provider_filter", ""));
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
