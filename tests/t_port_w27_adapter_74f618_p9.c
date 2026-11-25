/* JSON-lines harness for the w27_adapter_74f618_p9 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p9.h"

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
        if (!strcmp(func, "event_team_id")) out = w27_p9_event_team_id(in);
        else if (!strcmp(func, "team_in_install_set")) out = w27_p9_team_in_install_set(in);
        else if (!strcmp(func, "is_foreign_author")) out = w27_p9_is_foreign_author(in);
        else if (!strcmp(func, "context_channel_id")) out = w27_p9_context_channel_id(in);
        else if (!strcmp(func, "extract_assistant_meta")) out = w27_p9_extract_assistant_meta(in);
        else if (!strcmp(func, "agent_view_event_fields")) out = w27_p9_agent_view_event_fields(in);
        else if (!strcmp(func, "suggested_prompts")) out = w27_p9_suggested_prompts(in);
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
