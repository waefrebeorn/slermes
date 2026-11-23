#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_compression_e64919.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "timeout_fallback_prompt")) {
            char *s = w10_compression_e64919_timeout_fallback_prompt(
                json_get_str(in, "cached", ""),
                json_get_bool(in, "build_ok", false) ? 1 : 0,
                json_get_str(in, "built", ""),
                json_get_str(in, "system_message", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "warn_commit_overrun")) {
            char *s = w10_compression_e64919_warn_commit_overrun(
                json_get_num(in, "waited", 0.0), json_get_num(in, "ceiling", 0.0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "compression_timeout_warning")) {
            char *s = w10_compression_e64919_compression_timeout_warning(
                json_get_num(in, "idle", 0.0), json_get_num(in, "waited", 0.0),
                json_get_num(in, "total_ceiling", 0.0), json_get_num(in, "since_progress", 0.0),
                json_get_bool(in, "total_exhausted", false) ? 1 : 0,
                json_get_bool(in, "progress_observed", false) ? 1 : 0);
            out = json_string(s ? s : "");
            free(s);
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
