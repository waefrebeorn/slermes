#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_sessioncron_e77eee.h"

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
        if (!strcmp(func, "sched_exec")) {
            out = w7_sessioncron_e77eee_scheduler_execution_id(
                json_get_str(in, "job_id", ""), json_get_str(in, "request_id", ""));
        } else if (!strcmp(func, "validate")) {
            out = json_bool(w7_sessioncron_e77eee_validate_create_params(
                json_obj_get(in, "params")) ? true : false);
        } else if (!strcmp(func, "no_owner")) {
            out = w7_sessioncron_e77eee_no_owner_message(json_get_str(in, "chat_id", ""));
        } else if (!strcmp(func, "run_flags")) {
            out = w7_sessioncron_e77eee_run_job_flags(json_obj_get(in, "job"));
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
