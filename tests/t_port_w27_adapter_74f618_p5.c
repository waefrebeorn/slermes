/* JSON-lines harness for the w27_adapter_74f618_p5 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "timestamp_sort_key")) out = w27_p5_timestamp_sort_key(in);
        else if (!strcmp(func, "discard_oldest")) out = w27_p5_discard_oldest(in);
        else if (!strcmp(func, "evict_oldest")) out = w27_p5_evict_oldest(in);
        else if (!strcmp(func, "trim_dict_entries")) out = w27_p5_trim_dict_entries(in);
        else if (!strcmp(func, "metadata_team_id")) out = w27_p5_metadata_team_id(in);
        else if (!strcmp(func, "workspace_event_id")) out = w27_p5_workspace_event_id(in);
        else if (!strcmp(func, "workspace_message_marker")) out = w27_p5_workspace_message_marker(in);
        else if (!strcmp(func, "truthy_config")) out = w27_p5_truthy_config(in);
        else if (!strcmp(func, "is_ignored_channel")) out = w27_p5_is_ignored_channel(in);
        else if (!strcmp(func, "workspace_thread_key")) out = w27_p5_workspace_thread_key(in);
        else if (!strcmp(func, "agent_view_context_key")) out = w27_p5_agent_view_context_key(in);
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
