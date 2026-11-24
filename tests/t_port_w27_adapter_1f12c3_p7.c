/* JSON-lines harness for the w27_adapter_1f12c3_p7 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "allowed_user")) {
            out = w27_adapter_1f12c3_p7_is_allowed_user(in);
        }
        else if (!strcmp(func, "guild_role")) {
            out = w27_adapter_1f12c3_p7_guild_member_has_role(json_obj_get(in, "guild"), json_obj_get(in, "user_id"), json_obj_get(in, "allowed_roles"));
        }
        else if (!strcmp(func, "interaction_guild")) {
            out = w27_adapter_1f12c3_p7_interaction_guild_id(json_obj_get(in, "interaction"));
        }
        else if (!strcmp(func, "forum_parent")) {
            out = w27_adapter_1f12c3_p7_is_forum_parent(json_obj_get(in, "channel"));
        }
        else if (!strcmp(func, "parent_channel_id")) {
            out = w27_adapter_1f12c3_p7_get_parent_channel_id(json_obj_get(in, "channel"));
        }
        else if (!strcmp(func, "effective_topic")) {
            out = w27_adapter_1f12c3_p7_get_effective_topic(json_obj_get(in, "channel"), json_obj_get(in, "is_thread"));
        }
        else if (!strcmp(func, "attachment_type")) {
            out = w27_adapter_1f12c3_p7_attachment_message_type(json_obj_get(in, "att"));
        }
        else if (!strcmp(func, "auto_thread_name")) {
            out = w27_adapter_1f12c3_p7_derive_auto_thread_name(json_obj_get(in, "content"), json_obj_get(in, "default_name"));
        }
        else {
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
