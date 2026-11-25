#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_skillshub_a4dffc.h"

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
        if (!strcmp(func, "strip_html")) {
            out = w12_skillshub_a4dffc_strip_html(json_obj_get(in,"value"));
        } else if (!strcmp(func, "extract_repo_slug")) {
            out = w12_skillshub_a4dffc_extract_repo_slug(json_obj_get(in,"repo_value"));
        } else if (!strcmp(func, "normalize_identifier")) {
            out = w12_skillshub_a4dffc_normalize_identifier(json_obj_get(in,"identifier"));
        } else if (!strcmp(func, "wrap_identifier")) {
            out = w12_skillshub_a4dffc_wrap_identifier(json_obj_get(in,"identifier"));
        } else if (!strcmp(func, "detail_to_metadata")) {
            out = w12_skillshub_a4dffc_detail_to_metadata(json_obj_get(in,"canonical"),
                                                           json_obj_get(in,"detail"));
        } else if (!strcmp(func, "token_variants")) {
            out = w12_skillshub_a4dffc_token_variants(json_obj_get(in,"value"));
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
