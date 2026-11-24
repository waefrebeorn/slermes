/* JSON-lines harness for the w30_hostedroom_fe22db_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_hostedroom_fe22db_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, const json_t *in) {
    if (!strcmp(func, "metadata")) return w30_hostedroom_fe22db_p1_metadata(in);
    if (!strcmp(func, "blob_id_ok")) return w30_hostedroom_fe22db_p1_blob_id_ok(in);
    if (!strcmp(func, "blob_path")) return w30_hostedroom_fe22db_p1_blob_path(in);
    if (!strcmp(func, "owner_event")) return w30_hostedroom_fe22db_p1_owner_event(in);
    if (!strcmp(func, "named_by_owner_event"))
        return w30_hostedroom_fe22db_p1_named_by_owner_event(in);
    if (!strcmp(func, "sweep_select")) return w30_hostedroom_fe22db_p1_sweep_select(in);
    if (!strcmp(func, "prune_plan")) return w30_hostedroom_fe22db_p1_prune_plan(in);
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(func, in);
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
