/* JSON-lines harness for the w20_adapterhtm_fd926b gate.
 * One request object per stdin line; one response per line with "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_adapterhtm_fd926b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "latex")) {
            out = w20_adapterhtm_fd926b_latex_to_tokens(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "mx_maths")) {
            char *s = w20_adapterhtm_fd926b_tokens_to_mx_maths(
                json_get_str(in, "html", ""), json_obj_get(in, "store"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "tex_source")) {
            char *s = w20_adapterhtm_fd926b_tokens_to_tex_source(
                json_get_str(in, "text", ""), json_obj_get(in, "store"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "sanitize")) {
            char *s = w20_adapterhtm_fd926b_sanitize(
                json_get_str(in, "html", ""), json_obj_get(in, "store"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "pre_sanitize")) {
            char *s = w20_adapterhtm_fd926b_pre_sanitize_markdown(json_get_str(in, "text", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "safe_url")) {
            char *s = w20_adapterhtm_fd926b_safe_url(json_get_str(in, "value", ""));
            out = json_string(s ? s : "");
            free(s);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
