#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_skillshub_121926.h"

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
        if (!strcmp(func, "query_to_index_url")) {
            tmp = w11_skillshub_121926_query_to_index_url(jstr(json_obj_get(in,"query")));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "wrap_identifier")) {
            tmp = w11_skillshub_121926_wrap_identifier(jstr(json_obj_get(in,"base_url")),
                                                       jstr(json_obj_get(in,"skill_name")));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "parse_wellknown_identifier")) {
            out = w11_skillshub_121926_parse_wellknown_identifier(jstr(json_obj_get(in,"identifier")));
        } else if (!strcmp(func, "is_valid_skill_name")) {
            out = json_bool(w11_skillshub_121926_is_valid_skill_name(json_obj_get(in,"name")));
        } else if (!strcmp(func, "resolve_skill_name")) {
            tmp = w11_skillshub_121926_resolve_skill_name(json_obj_get(in,"fm"),
                                                          jstr(json_obj_get(in,"url")));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "url_matches")) {
            out = json_bool(w11_skillshub_121926_url_matches(json_obj_get(in,"identifier")));
        } else if (!strcmp(func, "agent_id")) {
            tmp = w11_skillshub_121926_agent_id(jstr(json_obj_get(in,"identifier")));
            out = str_or_null(tmp);
        } else if (!strcmp(func, "slug_from_identifier")) {
            tmp = w11_skillshub_121926_slug_from_identifier(jstr(json_obj_get(in,"identifier")));
            out = str_or_null(tmp);
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
