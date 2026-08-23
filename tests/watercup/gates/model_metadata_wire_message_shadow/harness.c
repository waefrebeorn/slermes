/* gate: model_metadata_wire_message_shadow — faithful traced copy of
 * src/agent/model_metadata.c::mm_wire_message_shadow
 * (LIVE twin: agent/model_metadata.py::_wire_message_shadow).
 *
 * Input: JSON object string (one message). Provider-independent.
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1 input not a JSON object        -> V("raw-passthrough")
 *   per key (insertion order): LOOP, then
 *   B2 k in {_anthropic_content_blocks, reasoning_details} -> V("skip")
 *   B3 k == "api_content" -> {
 *        B3a sidecar_wins             -> V("api_content->content")
 *        else                         -> V("api_content-drop") }
 *   B4 k == "content" -> {
 *        B4a sidecar_wins (skip clean) -> V("content-skip")
 *        B4b value is array -> per-part LOOP + B(image type) +
 *                              V("content-array:<n>:<marks>")
 *        B4c value is object with truthy _multimodal -> {
 *             B4d text_summary is string -> V("content-summary")
 *             else                       -> V("content-copy") }
 *        else                            -> V("content-copy") }
 *   else                               -> V("copy")
 *
 * Input restriction (documented): messages contain no "timestamp" key (the
 * LIVE version also drops PERSISTENCE_ONLY_MESSAGE_FIELDS={"timestamp"};
 * the C port does not) and multimodal dicts always carry a string
 * text_summary (LIVE defaults it to "" when absent; C copies the whole dict).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "json.h"

static bool sidecar_wins(const json_t *msg) {
    json_t *sidecar = json_obj_get((json_t *)msg, "api_content");
    if (!sidecar || sidecar->type != JSON_STRING || !sidecar->str_val || !sidecar->str_val[0])
        return false;
    json_t *role = json_obj_get((json_t *)msg, "role");
    const char *rs = (role && role->type == JSON_STRING && role->str_val) ? role->str_val : "";
    return strcmp(rs, "user") == 0 || strcmp(rs, "assistant") == 0;
}

static void wc_trace_one(const char *msg_json) {
    WCUP_BEGIN("wire_message_shadow");
    json_t *msg = json_parse(msg_json ? msg_json : "", NULL);
    if (!msg || msg->type != JSON_OBJECT) {                 /* B1 */
        WCUP_BRANCH(1);
        WCUP_VALSTR("raw-passthrough");
        goto out;
    }
    WCUP_BRANCH(0);
    {
        bool wins = sidecar_wins(msg);
        for (size_t i = 0; i < msg->c.count; i++) {
            WCUP_LOOP_TICK();
            const char *k = msg->c.keys[i];
            json_t *v = msg->c.items[i];
            if (strcmp(k, "_anthropic_content_blocks") == 0 ||
                strcmp(k, "reasoning_details") == 0) {       /* B2 */
                WCUP_BRANCH(1);
                WCUP_VALSTR("skip");
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(k, "api_content") == 0) {             /* B3 */
                WCUP_BRANCH(1);
                if (wins) {                                  /* B3a */
                    WCUP_BRANCH(1);
                    WCUP_VALSTR("api_content->content");
                } else {
                    WCUP_BRANCH(0);
                    WCUP_VALSTR("api_content-drop");
                }
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(k, "content") == 0) {                 /* B4 */
                WCUP_BRANCH(1);
                if (wins) {                                  /* B4a */
                    WCUP_BRANCH(1);
                    WCUP_VALSTR("content-skip");
                    continue;
                }
                WCUP_BRANCH(0);
                if (v && v->type == JSON_ARRAY) {            /* B4b */
                    WCUP_BRANCH(1);
                    char marks[512] = "";
                    size_t mi = 0;
                    for (size_t j = 0; j < v->c.count; j++) {
                        WCUP_LOOP_TICK();
                        json_t *part = v->c.items[j];
                        bool is_img = false;
                        if (part && part->type == JSON_OBJECT) {
                            json_t *pt_node = json_obj_get(part, "type");
                            const char *pt = (pt_node && pt_node->type == JSON_STRING &&
                                              pt_node->str_val) ? pt_node->str_val : "";
                            is_img = strcmp(pt, "image") == 0 ||
                                     strcmp(pt, "image_url") == 0 ||
                                     strcmp(pt, "input_image") == 0;
                        }
                        WCUP_BRANCH(is_img);
                        if (mi + 8 < sizeof marks) {
                            if (mi) marks[mi++] = ',';
                            mi += (size_t)snprintf(marks + mi, sizeof marks - mi,
                                                   "%s", is_img ? "STRIP" : "keep");
                        }
                    }
                    char buf[600];
                    snprintf(buf, sizeof buf, "content-array:%zu:%s", v->c.count, marks);
                    WCUP_VALSTR(buf);
                } else {
                    WCUP_BRANCH(0);
                    json_t *mm = (v && v->type == JSON_OBJECT)
                                 ? json_obj_get(v, "_multimodal") : NULL;
                    if (mm && mm->type == JSON_BOOL && mm->bool_val) {  /* B4c */
                        WCUP_BRANCH(1);
                        json_t *summary = json_obj_get(v, "text_summary");
                        if (summary && summary->type == JSON_STRING) {  /* B4d */
                            WCUP_BRANCH(1);
                            WCUP_VALSTR("content-summary");
                        } else {
                            WCUP_BRANCH(0);
                            WCUP_VALSTR("content-copy");
                        }
                    } else {
                        WCUP_BRANCH(0);
                        WCUP_VALSTR("content-copy");
                    }
                }
                continue;
            }
            WCUP_BRANCH(0);
            WCUP_VALSTR("copy");
        }
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wire_message_shadow");
    }
    if (msg) json_free(msg);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_shadow <msg-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_trace_one(argv[i]);
    return 0;
}
