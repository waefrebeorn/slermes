#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_contextfil_b3890c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "estimate")) {
            long long v = w13_contextfil_b3890c_estimate_tokens_rough(json_get_str(in, "text", ""));
            out = json_number((double)v);
        } else if (!strcmp(func, "entry")) {
            out = w13_contextfil_b3890c_entry(
                json_get_str(in, "label", ""), json_get_str(in, "path", ""),
                json_get_str(in, "content", ""), json_get_str(in, "status", ""));
        } else if (!strcmp(func, "loaded_status")) {
            int hit = json_get_bool(in, "scan_hit", false) ? 1 : 0;
            int ua = json_get_bool(in, "user_authored", false) ? 1 : 0;
            long long rl = strtoll(json_get_str(in, "rendered_len", "0"), NULL, 10);
            long long mc = strtoll(json_get_str(in, "max_chars", "0"), NULL, 10);
            char *s = w13_contextfil_b3890c_loaded_status(hit, rl, mc, ua);
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "render_lines")) {
            out = w13_contextfil_b3890c_render_context_file_lines(json_obj_get(in, "sources"));
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
