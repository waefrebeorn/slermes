#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_providermed_9f3e4f.h"

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
        if (!strcmp(func, "subdir")) {
            out = w7_providermed_9f3e4f_generated_subdir();
        } else if (!strcmp(func, "max_age")) {
            out = w7_providermed_9f3e4f_media_cache_max_age_hours();
        } else if (!strcmp(func, "max_redirects")) {
            out = w7_providermed_9f3e4f_max_save_url_redirects();
        } else if (!strcmp(func, "redirect")) {
            out = w7_providermed_9f3e4f_is_redirect_status((int)json_get_num(in, "code", 0));
        } else if (!strcmp(func, "cache_path")) {
            out = w7_providermed_9f3e4f_cache_path_ex(
                json_get_str(in, "home", ""), json_get_str(in, "kind", ""),
                json_get_str(in, "prefix", ""), json_get_str(in, "ts", ""),
                json_get_str(in, "short", ""), json_get_str(in, "extension", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
