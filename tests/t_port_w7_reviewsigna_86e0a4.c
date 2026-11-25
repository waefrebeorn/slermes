/* Port gate harness for agent/review_signals.py (gate w7_reviewsigna_86e0a4). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_reviewsigna_86e0a4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int has_skill_manage(const json_t *in) {
    json_t *names = json_obj_get(in, "valid_tool_names");
    if (!json_is_array(names)) return 0;
    for (size_t i = 0; i < json_len(names); i++) {
        const json_t *v = json_get(names, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, "skill_manage")) return 1;
    }
    return 0;
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
        if (!strcmp(func, "skill_review_due")) {
            out = json_bool(w7_reviewsigna_86e0a4_skill_review_due(
                json_get_str(in, "iters", "0"), json_get_str(in, "interval", "0"),
                has_skill_manage(in)));
        } else if (!strcmp(func, "background_review_enabled")) {
            out = json_bool(w7_reviewsigna_86e0a4_background_review_enabled(
                json_get_bool(in, "skip", false), json_get_str(in, "delegate_depth", "0"),
                json_get_bool(in, "configured", false)));
        } else if (!strcmp(func, "build_review_signals")) {
            out = w7_reviewsigna_86e0a4_build_review_signals(
                json_get_str(in, "iters", "0"), json_get_str(in, "turns", "0"),
                json_get_bool(in, "memory_review_due", false),
                json_get_bool(in, "skill_review_due", false),
                json_get_bool(in, "background_enabled", false),
                json_get_str(in, "skill_interval", "0"),
                json_get_str(in, "memory_interval", "0"));
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
