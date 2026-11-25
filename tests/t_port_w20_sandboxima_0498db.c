/* JSON-lines harness for the w20_sandboxima_0498db gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_sandboxima_0498db.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "image_pinned")) {
            out = json_bool(w20_sandboxima_0498db_image_pinned(
                                json_obj_get(in, "terminal_cfg"),
                                json_obj_get(in, "raw_terminal"),
                                json_obj_get(in, "scope"),
                                json_obj_get(in, "env_image")) ? 1 : 0);
        } else if (!strcmp(func, "pending")) {
            out = w20_sandboxima_0498db_pending(json_obj_get(in, "config"),
                                                json_obj_get(in, "raw_terminal"),
                                                json_obj_get(in, "scope"),
                                                json_obj_get(in, "env_image"),
                                                json_get_bool(in, "docker_found", true) ? 1 : 0,
                                                json_obj_get(in, "listing"));
        } else if (!strcmp(func, "explain_lines")) {
            out = w20_sandboxima_0498db_explain_lines(json_obj_get(in, "switch"));
        } else if (!strcmp(func, "decide_image")) {
            out = json_string(w20_sandboxima_0498db_decide_image(
                                  json_obj_get(in, "switch"),
                                  json_get_bool(in, "approve", false) ? 1 : 0));
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
