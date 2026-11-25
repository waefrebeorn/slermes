/* JSON-lines harness for the w22_promptreso_7bfabf gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_promptreso_7bfabf.h"

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
        if (!strcmp(func, "actor")) {
            out = w22_promptreso_7bfabf_actor(json_obj_get(in, "source"),
                                              json_obj_get(in, "admin_ids"),
                                              json_get_bool(in, "admitted", 1));
        } else if (!strcmp(func, "refusal_plan")) {
            out = w22_promptreso_7bfabf_refusal_plan(json_obj_get(in, "state"),
                                                     json_obj_get(in, "actor"),
                                                     json_get_str(in, "option_id", NULL),
                                                     json_get_str(in, "requester", NULL));
        } else if (!strcmp(func, "exec_ack")) {
            out = w22_promptreso_7bfabf_exec_ack(json_get_str(in, "option_id", NULL),
                                                 (int)json_get_num(in, "count", 0),
                                                 json_obj_get(in, "actor"));
        } else if (!strcmp(func, "slash_ack")) {
            out = w22_promptreso_7bfabf_slash_ack(json_get_str(in, "option_id", NULL),
                                                  json_get_str(in, "result_text", NULL));
        } else if (!strcmp(func, "clarify")) {
            out = w22_promptreso_7bfabf_clarify(json_get_str(in, "option_id", NULL),
                                                json_obj_get(in, "choices"));
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
