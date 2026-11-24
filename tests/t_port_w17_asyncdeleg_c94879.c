/* t_port_w17_asyncdeleg_c94879.c — harness for src/tools/async_delegation_worker.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_asyncdeleg_c94879.h"

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
        const char *did = json_get_str(in, "delegation_id", "");
        const char *cid = json_get_str(in, "claim_id", "");
        json_t *out = NULL;
        if (!strcmp(func, "complete"))
            out = w17_asyncdeleg_c94879_complete(json_obj_get(in, "event"),
                        json_obj_get(in, "result"), json_obj_get(in, "delivery"));
        else if (!strcmp(func, "child"))
            out = w17_asyncdeleg_c94879_child(did, json_obj_get(in, "entry"));
        else if (!strcmp(func, "read"))
            out = w17_asyncdeleg_c94879_read(did);
        else if (!strcmp(func, "claim"))
            out = w17_asyncdeleg_c94879_claim(did, cid);
        else if (!strcmp(func, "ack"))
            out = w17_asyncdeleg_c94879_ack(did, cid);
        else if (!strcmp(func, "defer"))
            out = w17_asyncdeleg_c94879_defer(did, cid);
        else if (!strcmp(func, "release"))
            out = w17_asyncdeleg_c94879_release(did, cid);
        else if (!strcmp(func, "drop"))
            out = w17_asyncdeleg_c94879_drop(did, cid);
        else if (!strcmp(func, "unscheduled"))
            out = w17_asyncdeleg_c94879_unscheduled(did);
        else
            out = json_string("UNKNOWN_FUNC");
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
