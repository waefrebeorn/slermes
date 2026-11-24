/* JSON-lines harness for the w32_hostedroom_be7d8d_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_hostedroom_be7d8d_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
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
        if (!strcmp(func, "text")) {
            out = w32_hostedroom_be7d8d_p2_text(json_obj_get(in, "mapping"), json_get_str(in, "key", ""),
                                                json_get_str(in, "value_type", "int"));
        } else if (!strcmp(func, "apply_handler")) {
            out = w32_hostedroom_be7d8d_p2_apply_handler(opt_str(in, "kind"));
        } else if (!strcmp(func, "named_sql")) {
            out = w32_hostedroom_be7d8d_p2_named_sql(opt_str(in, "name"));
        } else if (!strcmp(func, "schema_ddl")) {
            out = w32_hostedroom_be7d8d_p2_schema_ddl();
        } else if (!strcmp(func, "reopened")) {
            out = w32_hostedroom_be7d8d_p2_reopened();
        } else if (!strcmp(func, "publication_query")) {
            out = w32_hostedroom_be7d8d_p2_publication_query(opt_str(in, "status"));
        } else if (!strcmp(func, "event_seqs")) {
            out = w32_hostedroom_be7d8d_p2_event_seqs(json_obj_get(in, "source_event_seq"),
                                                      json_obj_get(in, "event_seqs"));
        } else if (!strcmp(func, "settled_check")) {
            out = w32_hostedroom_be7d8d_p2_settled_check(json_obj_get(in, "row"),
                                                         json_get_str(in, "discussion_event_id", ""));
        } else if (!strcmp(func, "receipt")) {
            out = w32_hostedroom_be7d8d_p2_receipt(json_obj_get(in, "kind"), json_obj_get(in, "payload"));
        } else if (!strcmp(func, "transcript_retention")) {
            out = w32_hostedroom_be7d8d_p2_transcript_retention(json_obj_get(in, "seqs"));
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
