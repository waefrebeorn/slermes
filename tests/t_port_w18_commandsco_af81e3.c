#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_commandsco_af81e3.h"

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
        char *s;
        if (!strcmp(func, "split_args")) {
            out = w18_commandsco_af81e3_split_args(json_get_str(in, "sub_text", ""));
        } else if (!strcmp(func, "extract_path_word")) {
            s = w18_commandsco_af81e3_extract_path_word(json_get_str(in, "text", ""));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "score_path")) {
            out = json_number((double)w18_commandsco_af81e3_score_path(
                json_get_str(in, "filepath", ""), json_get_str(in, "query", "")));
        } else if (!strcmp(func, "normalize_skill_token")) {
            s = w18_commandsco_af81e3_normalize_skill_token(json_get_str(in, "token", ""));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "completion_text")) {
            s = w18_commandsco_af81e3_completion_text(
                json_get_str(in, "cmd_name", ""), json_get_str(in, "word", ""));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "extract_context_word")) {
            s = w18_commandsco_af81e3_extract_context_word(json_get_str(in, "text", ""));
            out = s ? json_string(s) : json_null(); free(s);
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
