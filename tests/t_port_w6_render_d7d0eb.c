/* Port gate harness for tui_gateway/render.py (gate w6_render_d7d0eb). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_render_d7d0eb.h"

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
        int mp = json_get_bool(in, "module_present", true) ? 1 : 0;
        int ap = json_get_bool(in, "attr_present", true) ? 1 : 0;
        double cols = json_get_num(in, "cols", 80);
        const char *behavior = json_get_str(in, "behavior", "ok");
        const char *sentinel = json_get_str(in, "sentinel", "");
        json_t *out = NULL;
        if (!strcmp(func, "render_message")) {
            out = tuirnd_render_message(json_get_str(in, "text", ""), cols, mp, ap, behavior, sentinel);
        } else if (!strcmp(func, "render_diff")) {
            out = tuirnd_render_diff(json_get_str(in, "text", ""), cols, mp, ap, behavior, sentinel);
        } else if (!strcmp(func, "make_stream_renderer")) {
            out = tuirnd_make_stream_renderer(cols, mp, ap, behavior, sentinel);
        } else if (!strcmp(func, "invoke")) {
            out = tuirnd_invoke(json_get_str(in, "name", "format_response"),
                                json_get_str(in, "text", ""), 1, cols, mp, ap, behavior, sentinel);
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
