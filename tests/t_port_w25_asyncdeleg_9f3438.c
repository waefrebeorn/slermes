/* JSON-lines harness for the w25_asyncdeleg_9f3438 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_asyncdeleg_9f3438.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long as_ll(const json_t *v, long long def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return def;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "tail_text")) {
            out = w25_asyncdeleg_9f3438_tail_text(
                json_obj_get(in, "raw"),
                as_ll(json_obj_get(in, "lines"), W25_ASYNCDELEG_9F3438_TAIL_LINES),
                as_ll(json_obj_get(in, "chars"), W25_ASYNCDELEG_9F3438_TAIL_CHARS));
        } else if (!strcmp(func, "tails")) {
            out = w25_asyncdeleg_9f3438_tails(json_obj_get(in, "contents"));
        } else if (!strcmp(func, "git_hint")) {
            out = w25_asyncdeleg_9f3438_git_hint(json_obj_get(in, "head"),
                                                 json_obj_get(in, "branch"),
                                                 json_obj_get(in, "status"),
                                                 json_obj_get(in, "cwd"),
                                                 json_obj_get(in, "cwd_is_dir"));
        } else if (!strcmp(func, "constants")) {
            out = w25_asyncdeleg_9f3438_constants();
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
