/* JSON-lines harness for the w25_rangeshift_920f52 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_rangeshift_920f52.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* int(v) for the line-number shapes the fixtures use. */
static int as_int(const json_t *v, int def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (int)(long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        long parsed = strtol(v->str_val, &end, 10);
        if (end != v->str_val && *end == '\0') return (int)parsed;
    }
    return def;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        const char *pre, *post;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        pre = json_get_str(in, "pre_text", "");
        post = json_get_str(in, "post_text", "");
        if (!strcmp(func, "shift_line")) {
            out = w25_rangeshift_920f52_shift_line(pre, post,
                                                  as_int(json_obj_get(in, "line"), 0));
        } else if (!strcmp(func, "shift_diag")) {
            out = w25_rangeshift_920f52_shift_diag(pre, post,
                                                   json_obj_get(in, "diag"));
        } else if (!strcmp(func, "shift_baseline")) {
            out = w25_rangeshift_920f52_shift_baseline(pre, post,
                                                       json_obj_get(in, "baseline"));
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
