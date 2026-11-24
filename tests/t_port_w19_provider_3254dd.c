#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_provider_3254dd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "domain_list")) {
            out = w19_provider_3254dd_coerce_domain_list(json_obj_get(in, "value"));
        } else if (!strcmp(func, "coerce")) {
            out = w19_provider_3254dd_coerce(json_get_str(in, "cast", ""),
                                             json_obj_get(in, "value"),
                                             json_obj_get(in, "default"));
        } else if (!strcmp(func, "tool")) {
            out = w19_provider_3254dd_web_search_tool(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "prompt")) {
            char *s = w19_provider_3254dd_build_prompt(json_get_str(in, "query", ""),
                                                       (long long)json_get_num(in, "limit", 5));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "collect")) {
            out = w19_provider_3254dd_collect_output_text(json_obj_get(in, "resp"));
        } else if (!strcmp(func, "parse")) {
            out = w19_provider_3254dd_try_parse_json_results(json_get_str(in, "text", ""),
                                                             (long long)json_get_num(in, "limit", 5));
        } else if (!strcmp(func, "annotations")) {
            out = w19_provider_3254dd_results_from_annotations(json_obj_get(in, "annotations"),
                                                               json_get_str(in, "joined", ""),
                                                               (long long)json_get_num(in, "limit", 5));
        } else if (!strcmp(func, "extract")) {
            out = w19_provider_3254dd_extract_results(json_obj_get(in, "resp"),
                                                      (long long)json_get_num(in, "limit", 5));
        } else if (!strcmp(func, "web_config")) {
            out = w19_provider_3254dd_web_config(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "clamp")) {
            out = w19_provider_3254dd_limit_clamp(json_obj_get(in, "limit"));
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
