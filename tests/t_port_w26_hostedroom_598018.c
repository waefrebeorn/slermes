/* JSON-lines harness for the w26_hostedroom_598018 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_hostedroom_598018.h"

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
        if (!strcmp(func, "parse_ddl")) {
            const json_t *ddl = json_obj_get(in, "ddl");
            out = w26_hostedroom_598018_parse_ddl(
                (ddl && ddl->type == JSON_STRING) ? ddl->str_val : NULL);
        } else if (!strcmp(func, "required_columns")) {
            out = w26_hostedroom_598018_required_columns();
        } else if (!strcmp(func, "retire_sql")) {
            out = w26_hostedroom_598018_retire_sql(json_get_str(in, "where", ""));
        } else if (!strcmp(func, "event_bytes_backfill")) {
            out = w26_hostedroom_598018_event_bytes_backfill(json_get_str(in, "where", ""));
        } else if (!strcmp(func, "legacy_columns")) {
            out = w26_hostedroom_598018_legacy_columns();
        } else if (!strcmp(func, "remote_run_current")) {
            out = w26_hostedroom_598018_remote_run_current(json_obj_get(in, "columns"),
                                                           json_obj_get(in, "pk_rows"));
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
