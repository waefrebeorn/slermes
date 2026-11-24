/* JSON-lines harness for the w30_search_d40ff0_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_search_d40ff0_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "sanitize")) {
            out = w30_search_d40ff0_p1_sanitize(in);
        } else if (!strcmp(func, "quote_tokens")) {
            out = w30_search_d40ff0_p1_quote_tokens(in);
        } else if (!strcmp(func, "non_op_tokens")) {
            out = w30_search_d40ff0_p1_non_op_tokens(in);
        } else if (!strcmp(func, "strip_wildcards")) {
            out = w30_search_d40ff0_p1_strip_wildcards(in);
        } else if (!strcmp(func, "or_relaxed")) {
            out = w30_search_d40ff0_p1_or_relaxed(in);
        } else if (!strcmp(func, "trigram_eligible")) {
            out = w30_search_d40ff0_p1_trigram_eligible(in);
        } else if (!strcmp(func, "short_cjk")) {
            out = w30_search_d40ff0_p1_short_cjk(in);
        } else if (!strcmp(func, "trigram_route_ok")) {
            out = w30_search_d40ff0_p1_trigram_route_ok(in);
        } else if (!strcmp(func, "is_cjk")) {
            out = w30_search_d40ff0_p1_is_cjk(in);
        } else if (!strcmp(func, "contains_cjk")) {
            out = w30_search_d40ff0_p1_contains_cjk(in);
        } else if (!strcmp(func, "count_cjk")) {
            out = w30_search_d40ff0_p1_count_cjk(in);
        } else if (!strcmp(func, "escape_like")) {
            out = w30_search_d40ff0_p1_escape_like(in);
        } else if (!strcmp(func, "like_params")) {
            out = w30_search_d40ff0_p1_like_params(in);
        } else if (!strcmp(func, "flatten_text")) {
            out = w30_search_d40ff0_p1_flatten_text(in);
        } else if (!strcmp(func, "fields")) {
            out = w30_search_d40ff0_p1_fields(in);
        } else if (!strcmp(func, "backfill_info")) {
            out = w30_search_d40ff0_p1_backfill_info(in);
        } else if (!strcmp(func, "constants")) {
            out = w30_search_d40ff0_p1_constants();
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
