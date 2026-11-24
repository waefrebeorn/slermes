/* JSON-lines harness for the w21_runturnrun_bdbcf9 gate (approval timeout notice).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_runturnrun_bdbcf9.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "window")) {
            char *s = w21_runturnrun_bdbcf9_window(json_get_num(in, "seconds", 0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "notice")) {
            char *s = w21_runturnrun_bdbcf9_notice(json_get_num(in, "timeout_s", 0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "arm")) {
            out = w21_runturnrun_bdbcf9_arm(json_obj_get(in, "approval_data"),
                                            str_or(in, "session_key", ""),
                                            str_or(in, "fallback_reason", ""),
                                            json_get_num(in, "fallback_timeout", 300));
        } else if (!strcmp(func, "settle")) {
            out = w21_runturnrun_bdbcf9_settle(str_or(in, "reason", ""),
                                               json_obj_get(in, "still_current"),
                                               json_obj_get(in, "arm"),
                                               str_or(in, "command", ""),
                                               str_or(in, "card_message_id", ""));
        } else if (!strcmp(func, "card_body")) {
            char *s = w21_runturnrun_bdbcf9_card_body(str_or(in, "notice", ""),
                                                      str_or(in, "command", ""),
                                                      str_or(in, "reason", ""),
                                                      str_or(in, "measure", "len"),
                                                      json_get_num(in, "max_len", 4096));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "post_notice")) {
            out = w21_runturnrun_bdbcf9_post_notice(in);
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
