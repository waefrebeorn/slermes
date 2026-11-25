#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_browserses_71b99f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *str_or_null(char *s) {
    json_t *o = s ? json_string(s) : json_null();
    free(s);
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const json_t *sec = json_obj_get(in, "section");
        json_t *out = NULL;
        if (!strcmp(func, "command_timeout")) {
            out = json_number((double)w15_browserses_71b99f_command_timeout(sec));
        } else if (!strcmp(func, "inactivity_timeout")) {
            out = json_number((double)w15_browserses_71b99f_inactivity_timeout(
                sec, (long long)json_get_num(in, "default", 0)));
        } else if (!strcmp(func, "chromium_args")) {
            out = w15_browserses_71b99f_chromium_args(sec);
        } else if (!strcmp(func, "executable_path")) {
            out = str_or_null(w15_browserses_71b99f_executable_path(sec));
        } else if (!strcmp(func, "browser_engine")) {
            out = str_or_null(w15_browserses_71b99f_browser_engine(sec));
        } else if (!strcmp(func, "is_headed")) {
            out = json_bool(w15_browserses_71b99f_is_headed(sec));
        } else if (!strcmp(func, "allow_private_urls")) {
            out = json_bool(w15_browserses_71b99f_allow_private_urls(sec));
        } else if (!strcmp(func, "auto_local_private")) {
            out = json_bool(w15_browserses_71b99f_auto_local_private(sec));
        } else if (!strcmp(func, "use_real_profile")) {
            out = json_bool(w15_browserses_71b99f_use_real_profile(sec));
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
