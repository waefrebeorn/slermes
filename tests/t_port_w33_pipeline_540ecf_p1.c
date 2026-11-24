/* JSON-lines harness for the w33_pipeline_540ecf_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_pipeline_540ecf_p1.h"

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
        if (!strcmp(func, "config")) {
            out = w33_pipeline_540ecf_p1_config(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "organizer_user_id")) {
            out = w33_pipeline_540ecf_p1_organizer_user_id(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "parse_resource")) {
            out = w33_pipeline_540ecf_p1_parse_resource(in);
        } else if (!strcmp(func, "transcript_like")) {
            out = w33_pipeline_540ecf_p1_transcript_like(in);
        } else if (!strcmp(func, "is_artifact")) {
            out = w33_pipeline_540ecf_p1_is_artifact(in);
        } else if (!strcmp(func, "meeting_ids")) {
            out = w33_pipeline_540ecf_p1_meeting_ids(json_obj_get(in, "notification"));
        } else if (!strcmp(func, "receipt_key")) {
            out = w33_pipeline_540ecf_p1_receipt_key(json_obj_get(in, "notification"));
        } else if (!strcmp(func, "run_gate")) {
            out = w33_pipeline_540ecf_p1_run_gate(json_obj_get(in, "job"));
        } else if (!strcmp(func, "find_dedupe")) {
            out = w33_pipeline_540ecf_p1_find_dedupe(in);
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
