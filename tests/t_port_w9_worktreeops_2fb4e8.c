#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_worktreeops_2fb4e8.h"

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
        if (!strcmp(func, "normalize_git_bash_path")) {
            out = w9_worktreeops_2fb4e8_normalize_git_bash_path(json_get_str(in, "p", ""));
        } else if (!strcmp(func, "prune_tier_hours")) {
            out = w9_worktreeops_2fb4e8_prune_tier_hours(
                json_get_str(in, "name", ""), json_get_num(in, "max_age_hours", 0.0));
        } else if (!strcmp(func, "path_is_within_root")) {
            out = w9_worktreeops_2fb4e8_path_is_within_root(
                json_get_str(in, "path", ""), json_get_str(in, "root", ""));
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
