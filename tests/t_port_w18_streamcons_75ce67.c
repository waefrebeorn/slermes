#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_streamcons_75ce67.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* NULL when text is absent or JSON null (the Python non-str pass-through case). */
static const char *jtext(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "escape")) {
            char *r = w18_streamcons_75ce67_escape_code_fences_for_display(jtext(in, "text"));
            out = r ? json_string(r) : json_null();
            free(r);
        } else if (!strcmp(func, "open_span")) {
            out = w18_streamcons_75ce67_open_code_fence_span(jtext(in, "text"),
                      (long long)json_get_num(in, "upto", 0.0));
        } else if (!strcmp(func, "ensure_closed")) {
            char *r = w18_streamcons_75ce67_ensure_closed_code_fences(jtext(in, "text"));
            out = r ? json_string(r) : json_null();
            free(r);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
