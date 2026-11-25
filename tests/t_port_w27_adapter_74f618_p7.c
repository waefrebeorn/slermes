/* JSON-lines harness for the w27_adapter_74f618_p7 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "resolve_thread_ts")) out = w27_p7_resolve_thread_ts(in);
        else if (!strcmp(func, "reactions_enabled")) out = w27_p7_reactions_enabled(in);
        else if (!strcmp(func, "extra_flag")) out = w27_p7_extra_flag(in);
        else if (!strcmp(func, "parse_users_info")) out = w27_p7_parse_users_info(in);
        else if (!strcmp(func, "reaction_triggers")) out = w27_p7_reaction_triggers(in);
        else if (!strcmp(func, "reaction_trigger_target")) out = w27_p7_reaction_trigger_target(in);
        else if (!strcmp(func, "first_file_share")) out = w27_p7_first_file_share(in);
        else if (!strcmp(func, "synthetic_reaction_event")) out = w27_p7_synthetic_reaction_event(in);
        else if (!strcmp(func, "build_identity_prompt")) out = w27_p7_build_identity_prompt(in);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
