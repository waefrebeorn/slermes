/* t_port_w18_schedulerd_20df75.c — harness for src/cron/scheduler_detached_worker.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_schedulerd_20df75.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "close_late_result")) {
            out = w18_schedulerd_20df75_close_late_result(
                json_get_bool(in, "result_raises", false),
                json_get_bool(in, "has_handle", false));
        } else if (!strcmp(func, "defer_teardown")) {
            out = w18_schedulerd_20df75_defer_teardown(
                json_get_bool(in, "future_none", false),
                json_get_bool(in, "future_done", false),
                json_get_bool(in, "session_db_truthy", false),
                json_get_bool(in, "invoke_callback", false));
        } else if (!strcmp(func, "reap_background")) {
            out = w18_schedulerd_20df75_reap_background(
                json_get_bool(in, "has_pid", false),
                json_get_str(in, "pid", ""));
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
