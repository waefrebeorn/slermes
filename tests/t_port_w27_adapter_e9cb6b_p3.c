/* JSON-lines harness for the w27_adapter_e9cb6b_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_e9cb6b_p3.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "exec_approval_tier")) {
            out = w27_adapter_exec_approval_tier(json_obj_get(in, "choices"),
                                                 json_obj_get(in, "smart_denied"));
        } else if (!strcmp(func, "approval_reaction")) {
            out = w27_adapter_approval_reaction(json_obj_get(in, "tier"));
        } else if (!strcmp(func, "approval_action")) {
            out = w27_adapter_approval_action(json_obj_get(in, "emoji"));
        } else if (!strcmp(func, "flat_picker_choices")) {
            out = w27_adapter_flat_picker_choices(json_obj_get(in, "providers"));
        } else if (!strcmp(func, "choice_picker_emoji")) {
            out = w27_adapter_choice_picker_emoji(json_obj_get(in, "choices"));
        } else if (!strcmp(func, "apply_relation_metadata")) {
            out = w27_adapter_apply_relation_metadata(json_obj_get(in, "content"),
                                                      json_obj_get(in, "reply_to"),
                                                      json_obj_get(in, "thread_id"));
        } else if (!strcmp(func, "protect_mention_regions")) {
            out = w27_adapter_protect_mention_regions(json_obj_get(in, "text"));
        } else if (!strcmp(func, "extract_outbound_mentions")) {
            out = w27_adapter_extract_outbound_mentions(json_obj_get(in, "text"));
        } else if (!strcmp(func, "has_outbound_room_mention")) {
            out = w27_adapter_has_outbound_room_mention(json_obj_get(in, "text"));
        } else if (!strcmp(func, "inject_outbound_mention_links")) {
            out = w27_adapter_inject_outbound_mention_links(json_obj_get(in, "text"));
        } else if (!strcmp(func, "is_bot_mentioned")) {
            out = w27_adapter_is_bot_mentioned(json_obj_get(in, "user_id"),
                                               json_obj_get(in, "body"),
                                               json_obj_get(in, "formatted_body"),
                                               json_obj_get(in, "mention_user_ids"));
        } else if (!strcmp(func, "user_localpart")) {
            out = w27_adapter_user_localpart(json_obj_get(in, "user_id"));
        } else if (!strcmp(func, "strip_mention")) {
            out = w27_adapter_strip_mention(json_obj_get(in, "user_id"),
                                            json_obj_get(in, "body"));
        } else if (!strcmp(func, "mxc_to_http")) {
            out = w27_adapter_mxc_to_http(json_obj_get(in, "homeserver"),
                                          json_obj_get(in, "mxc_url"));
        } else if (!strcmp(func, "sanitize_link_url")) {
            out = w27_adapter_sanitize_link_url(json_obj_get(in, "url"));
        } else if (!strcmp(func, "format_message")) {
            out = w27_adapter_format_message(json_obj_get(in, "content"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
