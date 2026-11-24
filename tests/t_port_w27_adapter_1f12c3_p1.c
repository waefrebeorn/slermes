/* JSON-lines harness for the w27_adapter_1f12c3_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p1.h"

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
        if (!strcmp(func, "image_ext")) {
            out = w27_adapter_1f12c3_p1_image_ext_from_content_type(json_obj_get(in, "content_type"));
        }
        else if (!strcmp(func, "md_link")) {
            out = w27_adapter_1f12c3_p1_format_discord_markdown_link(json_obj_get(in, "label"), json_obj_get(in, "url"));
        }
        else if (!strcmp(func, "transport")) {
            out = w27_adapter_1f12c3_p1_is_discord_transport_error(json_obj_get(in, "type_name"), json_obj_get(in, "text"));
        }
        else if (!strcmp(func, "truncate")) {
            out = w27_adapter_1f12c3_p1_truncate_discord_component_text(json_obj_get(in, "text"), json_obj_get(in, "limit"));
        }
        else if (!strcmp(func, "needs_members")) {
            out = w27_adapter_1f12c3_p1_needs_server_members_intent(json_obj_get(in, "allowed_user_ids"), json_obj_get(in, "allowed_role_ids"));
        }
        else if (!strcmp(func, "guidance")) {
            out = w27_adapter_1f12c3_p1_format_privileged_intents_guidance(json_obj_get(in, "needs_members"));
        }
        else if (!strcmp(func, "snowflake")) {
            out = w27_adapter_1f12c3_p1_discord_snowflake_time(json_obj_get(in, "snowflake"));
        }
        else if (!strcmp(func, "metadata_nonconv")) {
            out = w27_adapter_1f12c3_p1_metadata_marks_nonconversational(json_obj_get(in, "metadata"));
        }
        else if (!strcmp(func, "prompt_target")) {
            out = w27_adapter_1f12c3_p1_prompt_target_id(json_obj_get(in, "chat_id"), json_obj_get(in, "metadata"));
        }
        else if (!strcmp(func, "looks_nonconv")) {
            out = w27_adapter_1f12c3_p1_looks_like_nonconversational_history_message(json_obj_get(in, "content"));
        }
        else if (!strcmp(func, "clean_id")) {
            out = w27_adapter_1f12c3_p1_clean_discord_id(json_obj_get(in, "entry"));
        }
        else if (!strcmp(func, "ready_timeout")) {
            out = w27_adapter_1f12c3_p1_discord_ready_timeout_seconds(json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "env_bool")) {
            out = w27_adapter_1f12c3_p1_env_bool(json_obj_get(in, "env"), json_obj_get(in, "default"));
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
