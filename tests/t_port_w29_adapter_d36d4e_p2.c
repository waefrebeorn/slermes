/* JSON-lines harness for the w29_adapter_d36d4e_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_d36d4e_p2.h"

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
        if (!strcmp(func, "unwrap_sync_message")) {
            out = w29_adapter_d36d4e_p2_unwrap_sync_message(json_obj_get(in, "envelope_data"),
                                                             json_obj_get(in, "account"),
                                                             json_obj_get(in, "recent_sent"));
        } else if (!strcmp(func, "apply_group_mention_rules")) {
            out = w29_adapter_d36d4e_p2_apply_group_mention_rules(json_obj_get(in, "text"),
                                                                  json_obj_get(in, "data_message"),
                                                                  json_obj_get(in, "account"),
                                                                  json_obj_get(in, "require_mention"),
                                                                  json_obj_get(in, "uuid_by_number"));
        } else if (!strcmp(func, "group_allowed")) {
            out = w29_adapter_d36d4e_p2_group_allowed(json_obj_get(in, "group_id"),
                                                      json_obj_get(in, "allow"));
        } else if (!strcmp(func, "remember_recipient_identifiers")) {
            out = w29_adapter_d36d4e_p2_remember_recipient_identifiers(json_obj_get(in, "number"),
                                                                        json_obj_get(in, "service_id"),
                                                                        json_obj_get(in, "maps"));
        } else if (!strcmp(func, "extract_quote_author")) {
            out = w29_adapter_d36d4e_p2_extract_quote_author(json_obj_get(in, "quote"));
        } else if (!strcmp(func, "quote_references_own_message")) {
            out = w29_adapter_d36d4e_p2_quote_references_own_message(json_obj_get(in, "reply_to_id"),
                                                                      json_obj_get(in, "reply_to_author"),
                                                                      json_obj_get(in, "sent_timestamps"),
                                                                      json_obj_get(in, "account"),
                                                                      json_obj_get(in, "uuid_by_number"),
                                                                      json_obj_get(in, "number_by_uuid"));
        } else if (!strcmp(func, "remember_sent_message_timestamp")) {
            out = w29_adapter_d36d4e_p2_remember_sent_message_timestamp(json_obj_get(in, "cache"),
                                                                        json_obj_get(in, "timestamp"),
                                                                        json_obj_get(in, "max"));
        } else if (!strcmp(func, "extract_contact_uuid")) {
            out = w29_adapter_d36d4e_p2_extract_contact_uuid(json_obj_get(in, "contact"),
                                                             json_obj_get(in, "phone"));
        } else if (!strcmp(func, "consume_sent_timestamp")) {
            out = w29_adapter_d36d4e_p2_consume_sent_timestamp(json_obj_get(in, "recent"),
                                                               json_obj_get(in, "ts"));
        } else if (!strcmp(func, "envelope_decision")) {
            out = w29_adapter_d36d4e_p2_envelope_decision(json_obj_get(in, "envelope"),
                                                          json_obj_get(in, "account"),
                                                          json_obj_get(in, "allow"),
                                                          json_obj_get(in, "ignore_stories"),
                                                          json_obj_get(in, "require_mention"),
                                                          json_obj_get(in, "recent_sent"),
                                                          json_obj_get(in, "sent_cache"),
                                                          json_obj_get(in, "media_types"),
                                                          json_obj_get(in, "ignore_attachments"),
                                                          json_obj_get(in, "uuid_by_number"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        {
            char *f = json_dumps(json_string(func), 0);
            printf("\"func\":%s", f ? f : "\"\"");
            if (f) free(f);
        }
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
