/* gate: agent_bedrock_adapter_normalize_converse_response — links the FIXED
 * src/agent/bedrock_adapter_ports.c::brd_normalize_converse_response via
 * extra_src. LIVE twin: agent/bedrock_adapter.py::normalize_converse_response.
 *
 * Branch ladder (twin.py mirrors EXACTLY), per content block:
 *   B1: block has "text"      B2: reasoningContent with nonempty text
 *   B3: block has "toolUse"
 * then B4: tool_calls present && finish_reason == "stop" -> "tool_calls".
 * VSTR(content|reasoning|ncalls|finish|pt|ct|ttot|cr|cw|model) — read back
 * out of the REAL port output so the value parity covers the whole shape.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "watercup.h"

char *brd_normalize_converse_response(const char *response_json);

static void wc_case(const char *response_json) {
    WCUP_BEGIN("brd_normalize_converse_response");

    json_t *resp = json_parse(response_json, NULL);
    json_t *blocks = NULL;
    int n_tool_calls = 0;
    if (resp && resp->type == JSON_OBJECT) {
        json_t *output = json_obj_get(resp, "output");
        json_t *message = (output && output->type == JSON_OBJECT)
                              ? json_obj_get(output, "message") : NULL;
        json_t *content = (message && message->type == JSON_OBJECT)
                              ? json_obj_get(message, "content") : NULL;
        if (content && content->type == JSON_ARRAY) blocks = content;
    }
    size_t nblocks = blocks ? json_len(blocks) : 0;
    for (size_t i = 0; i < nblocks; i++) {
        json_t *block = json_get(blocks, (int)i);
        if (!block || block->type != JSON_OBJECT) continue;
        int has_text = json_obj_get(block, "text") != NULL;
        WCUP_BRANCH(has_text);                                  /* B1 */
        if (has_text) continue;
        json_t *rc = json_obj_get(block, "reasoningContent");
        const char *rt = (rc && rc->type == JSON_OBJECT)
                             ? json_get_str(rc, "text", "") : "";
        int has_reasoning = (rt && rt[0]);
        WCUP_BRANCH(has_reasoning);                             /* B2 */
        if (has_reasoning) continue;
        WCUP_BRANCH(0);
        int has_tu = json_obj_get(block, "toolUse") != NULL;
        WCUP_BRANCH(has_tu);                                    /* B3 */
        if (has_tu) n_tool_calls++;
    }

    char *out_json = brd_normalize_converse_response(response_json);
    json_t *out = json_parse(out_json, NULL);
    char label[2048];
    if (!out || out->type != JSON_OBJECT) {
        snprintf(label, sizeof label, "BAD_OUTPUT");
    } else {
        json_t *choice = json_get(json_obj_get(out, "choices"), 0);
        json_t *msg = choice ? json_obj_get(choice, "message") : NULL;
        json_t *usage = json_obj_get(out, "usage");
        json_t *content_v = msg ? json_obj_get(msg, "content") : NULL;
        json_t *reason_v = msg ? json_obj_get(msg, "reasoning_content") : NULL;
        json_t *tcs = msg ? json_obj_get(msg, "tool_calls") : NULL;
        int ncalls = (tcs && tcs->type == JSON_ARRAY) ? (int)json_len(tcs) : 0;
        const char *fr = choice ? json_get_str(choice, "finish_reason", "") : "";
        int fixup = (ncalls > 0 && strcmp(fr, "stop") == 0);
        WCUP_BRANCH(fixup);                                     /* B4 */
        snprintf(label, sizeof label, "%s|%s|%d|%s|%ld|%ld|%ld|%ld|%ld|%s",
                 (content_v && content_v->str_val) ? content_v->str_val : "",
                 (reason_v && reason_v->str_val) ? reason_v->str_val : "",
                 ncalls, fr,
                 (long)json_get_num(usage, "prompt_tokens", 0),
                 (long)json_get_num(usage, "completion_tokens", 0),
                 (long)json_get_num(usage, "total_tokens", 0),
                 (long)json_get_num(usage, "cache_read_input_tokens", 0),
                 (long)json_get_num(usage, "cache_creation_input_tokens", 0),
                 json_get_str(out, "model", ""));
        (void)n_tool_calls;
    }
    WCUP_VALSTR(label);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("brd_normalize_converse_response");
    }
    if (out) json_free(out);
    free(out_json);
    if (resp) json_free(resp);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s brd_normalize_converse_response <response-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_case(argv[i]);
    return 0;
}
