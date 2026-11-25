/* JSON-lines harness for the w20_telegramfl_e6bdc8 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_telegramfl_e6bdc8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bot_id")) {
            out = w20_telegramfl_e6bdc8_bot_id(json_get_str(in, "token", NULL));
        } else if (!strcmp(func, "store_name")) {
            out = w20_telegramfl_e6bdc8_store_name(json_get_str(in, "token", NULL),
                                                   json_get_bool(in, "has_receipt_dir", 0));
        } else if (!strcmp(func, "seed")) {
            out = w20_telegramfl_e6bdc8_seed_windows(json_get_str(in, "token", NULL),
                                                     json_get_bool(in, "has_receipt_dir", 1),
                                                     json_get_str(in, "saved_text", NULL),
                                                     json_obj_get(in, "preset"),
                                                     json_get_bool(in, "loaded", 0),
                                                     json_get_num(in, "wall_now", 0.0),
                                                     json_get_num(in, "loop_now", 0.0));
        } else if (!strcmp(func, "record")) {
            out = w20_telegramfl_e6bdc8_record(json_obj_get(in, "chat"),
                                               json_get_num(in, "wait", 0.0),
                                               json_get_num(in, "loop_now", 0.0),
                                               json_obj_get(in, "preset"));
        } else if (!strcmp(func, "remaining")) {
            out = w20_telegramfl_e6bdc8_remaining(json_obj_get(in, "until"),
                                                  json_obj_get(in, "chat"),
                                                  json_get_num(in, "loop_now", 0.0));
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
