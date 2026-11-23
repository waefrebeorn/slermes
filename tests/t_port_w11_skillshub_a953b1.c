#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillshub_a953b1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static json_t *str_or_null(const char *s){ return s ? json_string(s) : json_null(); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func, "query_terms")) {
            out = w11_skillshub_a953b1_query_terms(jstr(json_obj_get(in,"query")));
        } else if (!strcmp(func, "search_score")) {
            int score = w11_skillshub_a953b1_search_score(jstr(json_obj_get(in,"query")),
                                                          json_obj_get(in,"meta"));
            out = json_number((double)score);
        } else if (!strcmp(func, "normalize_tags")) {
            out = w11_skillshub_a953b1_normalize_tags(json_obj_get(in,"tags"));
        } else if (!strcmp(func, "owner_from_payload")) {
            tmp = w11_skillshub_a953b1_owner_from_payload(json_obj_get(in,"data"));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "owner_matches")) {
            out = json_bool(w11_skillshub_a953b1_owner_matches(jstr(json_obj_get(in,"expected_owner")),
                                                               json_obj_get(in,"data")));
        } else if (!strcmp(func, "parse_identifier")) {
            out = w11_skillshub_a953b1_parse_identifier(jstr(json_obj_get(in,"identifier")));
        } else if (!strcmp(func, "catalog_identifier")) {
            tmp = w11_skillshub_a953b1_catalog_identifier(json_obj_get(in,"identifier"));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "first_str")) {
            tmp = w11_skillshub_a953b1_first_str(json_obj_get(in,"values"));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "coerce_skill_payload")) {
            out = w11_skillshub_a953b1_coerce_skill_payload(json_obj_get(in,"data"));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
