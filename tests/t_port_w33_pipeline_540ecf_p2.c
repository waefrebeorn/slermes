/* JSON-lines harness for the w33_pipeline_540ecf_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_pipeline_540ecf_p2.h"

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
        if (!strcmp(func, "rich_text")) {
            out = w33_pipeline_540ecf_p2_rich_text(in);
        } else if (!strcmp(func, "bullets")) {
            out = w33_pipeline_540ecf_p2_bullets(json_obj_get(in, "items"));
        } else if (!strcmp(func, "sections")) {
            out = w33_pipeline_540ecf_p2_sections(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "notion_properties")) {
            out = w33_pipeline_540ecf_p2_notion_properties(in);
        } else if (!strcmp(func, "notion_blocks")) {
            out = w33_pipeline_540ecf_p2_notion_blocks(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "linear_body")) {
            out = w33_pipeline_540ecf_p2_linear_body(in);
        } else if (!strcmp(func, "render_markdown")) {
            out = w33_pipeline_540ecf_p2_render_markdown(in);
        } else if (!strcmp(func, "summary_prompt")) {
            out = w33_pipeline_540ecf_p2_summary_prompt(in);
        } else if (!strcmp(func, "parse_summary")) {
            out = w33_pipeline_540ecf_p2_parse_summary(in);
        } else if (!strcmp(func, "heuristic_summary")) {
            out = w33_pipeline_540ecf_p2_heuristic_summary(in);
        } else if (!strcmp(func, "participants")) {
            out = w33_pipeline_540ecf_p2_participants(json_obj_get(in, "meeting_ref"));
        } else if (!strcmp(func, "call_metrics")) {
            out = w33_pipeline_540ecf_p2_call_metrics(json_obj_get(in, "artifacts"));
        } else if (!strcmp(func, "payload_targets")) {
            out = w33_pipeline_540ecf_p2_payload_targets(in);
        } else if (!strcmp(func, "transcript_decision")) {
            out = w33_pipeline_540ecf_p2_transcript_decision(in);
        } else if (!strcmp(func, "recording_name")) {
            out = w33_pipeline_540ecf_p2_recording_name(in);
        } else if (!strcmp(func, "stt_outcome")) {
            out = w33_pipeline_540ecf_p2_stt_outcome(json_obj_get(in, "result"));
        } else if (!strcmp(func, "sink_plan")) {
            out = w33_pipeline_540ecf_p2_sink_plan(in);
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
