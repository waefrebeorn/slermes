/* JSON-lines harness for the w27_adapter_1f12c3_p4 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "backfill_enabled")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_enabled(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "backfill_number")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_number(json_obj_get(in, "extra"), json_obj_get(in, "key"), json_obj_get(in, "default"), json_obj_get(in, "cast"), json_obj_get(in, "lo"), json_obj_get(in, "hi"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "window_seconds")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_window_seconds(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "backfill_limit")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_limit(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "max_dispatches")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_max_dispatches(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "max_attempts")) {
            out = w27_adapter_1f12c3_p4_missed_message_backfill_max_attempts(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "down_notice")) {
            out = w27_adapter_1f12c3_p4_is_down_notice_content(json_obj_get(in, "content"));
        }
        else if (!strcmp(func, "channel_ids")) {
            out = w27_adapter_1f12c3_p4_message_channel_ids(json_obj_get(in, "channel"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
