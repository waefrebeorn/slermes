/* t_port_w17_delegateto_5314c5.c — harness for src/tools/delegate_tool_child_run.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_delegateto_5314c5.h"

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
        if (!strcmp(func, "num"))
            out = w17_delegateto_5314c5_num(json_obj_get(in, "value"), json_obj_get(in, "default"));
        else if (!strcmp(func, "str_or_none"))
            out = w17_delegateto_5314c5_str_or_none(json_obj_get(in, "value"));
        else if (!strcmp(func, "is_image_url"))
            out = w17_delegateto_5314c5_is_image_url(json_get_str(in, "ref", ""));
        else if (!strcmp(func, "budget_warning_text"))
            out = w17_delegateto_5314c5_budget_warning_text(json_get_num(in, "idle", 0),
                        json_get_num(in, "timeout", 0));
        else if (!strcmp(func, "fabricated_entry"))
            out = w17_delegateto_5314c5_fabricated_entry((int)json_get_num(in, "idx", 0),
                        json_get_str(in, "status", ""), json_obj_get(in, "error"),
                        json_obj_get(in, "child"), json_get_num(in, "duration", 0));
        else if (!strcmp(func, "append_missed_steer"))
            out = w17_delegateto_5314c5_append_missed_steer(json_obj_get(in, "entry"), json_obj_get(in, "late"));
        else if (!strcmp(func, "tier_in_effect"))
            out = w17_delegateto_5314c5_tier_in_effect(json_obj_get(in, "child"));
        else if (!strcmp(func, "constants"))
            out = w17_delegateto_5314c5_constants();
        else
            out = json_string("UNKNOWN_FUNC");
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
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
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
