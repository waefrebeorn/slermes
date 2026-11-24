#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_commandspl_3e5241.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "requires_argument"))
            out = w17_commandspl_3e5241_requires_argument(json_get_str(a, "args_hint", ""));
        else if (!strcmp(func, "sanitize_telegram_name"))
            out = w17_commandspl_3e5241_sanitize_telegram_name(json_get_str(a, "raw", ""));
        else if (!strcmp(func, "normalize_telegram_desc"))
            out = w17_commandspl_3e5241_normalize_telegram_desc(json_get_str(a, "desc", ""));
        else if (!strcmp(func, "truncate_desc"))
            out = w17_commandspl_3e5241_truncate_desc(json_get_str(a, "desc", ""),
                                                       (int)json_get_num(a, "limit", 20));
        else if (!strcmp(func, "clamp_command_names"))
            out = w17_commandspl_3e5241_clamp_command_names(json_obj_get(a, "entries"),
                                                             json_obj_get(a, "reserved"));
        else if (!strcmp(func, "sanitized_rank"))
            out = w17_commandspl_3e5241_sanitized_rank(json_obj_get(a, "raw_names"));
        else if (!strcmp(func, "sanitize_slack_name"))
            out = w17_commandspl_3e5241_sanitize_slack_name(json_get_str(a, "raw", ""));
        else out = json_string("UNKNOWN_FUNC");
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
