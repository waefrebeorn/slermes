#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_schedulerd_4551a7.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "standalone_send")) {
            out = w19_schedulerd_4551a7_standalone_send(
                json_get_str(in, "where", ""),
                json_get_str(in, "content", ""),
                (int)json_get_num(in, "media_count", 0),
                json_get_num(in, "send_timeout", 0.0),
                json_get_num(in, "base_timeout", 0.0),
                json_get_bool(in, "shutting_down", false),
                json_get_str(in, "outcome", "ok"),
                json_get_str(in, "exc_text", ""),
                json_get_bool(in, "request_never_left", false),
                json_get_bool(in, "in_flight0", false),
                json_get_bool(in, "shutdown_on_retry", false),
                json_obj_get(in, "ok_result"));
        } else if (!strcmp(func, "queue_for_reconnect")) {
            out = w19_schedulerd_4551a7_queue_for_reconnect(
                json_get_str(in, "where", ""),
                json_get_str(in, "platform_name", ""),
                json_get_str(in, "chat_id", ""),
                json_get_str(in, "thread_id", ""),
                json_get_str(in, "job_id", ""),
                json_get_str(in, "content", ""),
                (int)json_get_num(in, "media_count", 0),
                json_get_str(in, "live_error", ""),
                json_get_bool(in, "live_retryable", false),
                json_get_bool(in, "ledger_enabled", true),
                json_get_bool(in, "has_loop", false),
                json_get_bool(in, "has_schedule", false),
                json_get_bool(in, "record_raises", false),
                json_get_str(in, "adapter_profile", ""));
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
