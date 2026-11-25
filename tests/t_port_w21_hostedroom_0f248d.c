/* JSON-lines harness for the w21_hostedroom_0f248d gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_hostedroom_0f248d.h"
#include "tui_gateway/hosted_room_prepare_stall.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "describe")) {
            char *s = w21_hostedroom_0f248d_describe(json_get_str(in, "message", ""),
                                                     json_get_str(in, "type", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "feed")) {
            out = w21_hostedroom_0f248d_feed(json_obj_get(in, "events"));
        } else if (!strcmp(func, "retry_action")) {
            out = w21_hostedroom_0f248d_retry_action(json_get_str(in, "room_id", ""),
                                                     json_obj_get(in, "room"));
        } else if (!strcmp(func, "report_plan")) {
            out = w21_hostedroom_0f248d_report_plan(json_get_str(in, "latest_seq", "0"),
                                                    json_get_str(in, "tail_event_id", NULL),
                                                    json_get_str(in, "message", ""),
                                                    json_get_str(in, "type", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
