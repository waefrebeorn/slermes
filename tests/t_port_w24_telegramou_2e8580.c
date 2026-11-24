/* JSON-lines harness for the w24_telegramou_2e8580 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_telegramou_2e8580.h"

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
        if (!strcmp(func, "is_visually_empty")) {
            out = w24_telegramou_2e8580_is_visually_empty(json_obj_get(in, "text"));
        } else if (!strcmp(func, "notification_kwargs")) {
            out = w24_telegramou_2e8580_notification_kwargs(json_obj_get(in, "mode"),
                                                            json_obj_get(in, "notify"));
        } else if (!strcmp(func, "metadata")) {
            out = w24_telegramou_2e8580_metadata(json_obj_get(in, "thread_id"),
                                                 json_obj_get(in, "notify"));
        } else if (!strcmp(func, "resolve_mode")) {
            out = w24_telegramou_2e8580_resolve_mode(json_obj_get(in, "env"),
                                                     json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "constants")) {
            out = w24_telegramou_2e8580_constants();
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
