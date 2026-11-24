#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_pruning_c083bd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "lock_path")) {
            char *s = w19_pruning_c083bd_store_lock_path(json_get_str(in, "base", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "parse_metadata")) {
            out = w19_pruning_c083bd_parse_metadata(json_get_str(in, "text", ""),
                                                    json_get_str(in, "sep", "|"),
                                                    json_get_str(in, "sha", "sha0"));
        } else if (!strcmp(func, "commits")) {
            out = w19_pruning_c083bd_commits(json_get_str(in, "output", ""),
                                             json_get_str(in, "ref", ""),
                                             opt_str(in, "git_error"));
        } else if (!strcmp(func, "trim")) {
            out = w19_pruning_c083bd_trim(json_obj_get(in, "commits"),
                                          (long long)json_get_num(in, "keep_count", 1),
                                          json_get_str(in, "ref", "refs/ck/x"));
        } else if (!strcmp(func, "drop")) {
            out = w19_pruning_c083bd_drop_one_round(json_obj_get(in, "cap"),
                                                    json_obj_get(in, "size"),
                                                    json_obj_get(in, "refs"));
        } else if (!strcmp(func, "enforce")) {
            out = w19_pruning_c083bd_enforce_size(json_obj_get(in, "cap"),
                                                  json_obj_get(in, "size"),
                                                  json_obj_get(in, "refs"),
                                                  json_obj_get(in, "sizes"));
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
