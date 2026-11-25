#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_managedwork_1ac730.h"

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
        if (!strcmp(func, "scalar")) {
            out = w7_managedwork_1ac730_scalar(json_obj_get(in, "value"));
        } else if (!strcmp(func, "structured")) {
            out = w7_managedwork_1ac730_structured(json_get_str(in, "key", ""), json_obj_get(in, "value"));
        } else if (!strcmp(func, "result_frame")) {
            out = w7_managedwork_1ac730_result_frame(json_obj_get(in, "result"));
        } else if (!strcmp(func, "accept_result")) {
            out = w7_managedwork_1ac730_accept_result(json_obj_get(in, "result"));
        } else if (!strcmp(func, "activity_interval")) {
            out = w7_managedwork_1ac730_activity_interval(json_get_num(in, "wedge_timeout", 0));
        } else if (!strcmp(func, "outbox_dir")) {
            out = w7_managedwork_1ac730_outbox_dir(json_get_str(in, "home", ""), json_get_str(in, "execution_id", NULL));
        } else if (!strcmp(func, "activity_frame_interval")) {
            out = w7_managedwork_1ac730_activity_frame_interval(json_get_str(in, "request_json", NULL));
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
