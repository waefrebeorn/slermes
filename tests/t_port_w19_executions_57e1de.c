/* t_port_w19_executions_57e1de.c - harness for src/persistence/sqlite/workers/executions.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_executions_57e1de.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "key_set")) {
            out = w19_executions_57e1de_key_set(json_obj_get(in, "keys"),
                    json_obj_get(in, "allowed"), json_obj_get(in, "required"));
        } else if (!strcmp(func, "messages")) {
            out = w19_executions_57e1de_messages(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "record")) {
            out = w19_executions_57e1de_record(json_obj_get(in, "public"));
        } else if (!strcmp(func, "payload_usage_main")) {
            out = w19_executions_57e1de_payload_usage_main(json_obj_get(in, "usage"),
                                                           json_obj_get(in, "allowed"));
        } else if (!strcmp(func, "payload_usage_auxiliary")) {
            out = w19_executions_57e1de_payload_usage_auxiliary(json_obj_get(in, "usage"),
                    json_get_str(in, "task", ""), json_obj_get(in, "allowed"));
        } else if (!strcmp(func, "payload_create")) {
            out = w19_executions_57e1de_payload_create(json_obj_get(in, "fields"),
                                                       json_obj_get(in, "allowed"));
        } else if (!strcmp(func, "payload_append")) {
            out = w19_executions_57e1de_payload_append(json_obj_get(in, "messages"),
                    json_obj_get(in, "options"), json_obj_get(in, "allowed"));
        } else if (!strcmp(func, "payload_user_content")) {
            out = w19_executions_57e1de_payload_user_content(json_obj_get(in, "row_id"),
                    json_obj_get(in, "content"), json_obj_get(in, "display_metadata"),
                    json_get_bool(in, "has_display_metadata", false) ? 1 : 0);
        } else if (!strcmp(func, "payload_restore_cooldown")) {
            out = w19_executions_57e1de_payload_restore_cooldown(json_obj_get(in, "snapshot"));
        } else if (!strcmp(func, "payload_lease")) {
            out = w19_executions_57e1de_payload_lease(json_get_str(in, "holder", ""),
                                                      json_obj_get(in, "ttl_seconds"));
        } else if (!strcmp(func, "payload_boundary")) {
            out = w19_executions_57e1de_payload_boundary(json_obj_get(in, "peer"));
        } else if (!strcmp(func, "payload_activity")) {
            out = w19_executions_57e1de_payload_activity(json_obj_get(in, "activity"));
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
