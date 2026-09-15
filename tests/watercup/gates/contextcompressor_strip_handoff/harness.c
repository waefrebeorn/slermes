/* gate: contextcompressor_strip_handoff — traced harness around the REAL
 * slermes src/agent/context_compressor_pure.c::
 * cc_strip_context_summary_handoff_message (linked via extra_src; the trace
 * sits at the call boundary: input classification + real result values).
 * LIVE twin: agent/context_compressor.py::
 * ContextCompressor._strip_context_summary_handoff_message.
 *
 * Rung contract (identical order both sides):
 *   B0 message is a JSON object
 *   V classification ("standalone"/"merged"/"none")   [cc_classify_summary_content]
 *   V has-metadata ("1"/"0")                          [cc_has_compressed_summary_metadata]
 *   B1 is_summary
 *   <call the REAL C strip fn>
 *   V result content string (or "<unchanged>")
 *   B2 result content differs from input content
 *
 * Agreed subset only (documented): inputs use STRING content. LIVE also
 * unwraps LIST-typed content blocks; the C port intentionally falls back to a
 * copy there, so list-content inputs are excluded from this gate's walk.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "context_compressor_pure.h"

/* Link-closure stubs: redaction / model-metadata are NOT on the traced path
 * (string-content strip never calls them); they only satisfy the linker for
 * unrelated code in the linked translation units. */
char *hermes_redact(const char *s) { return s ? strdup(s) : NULL; }
char *hermes_redact_force(const char *s) { return s ? strdup(s) : NULL; }
long get_model_context_length(const char *model) { (void)model; return 200000L; }

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_handoff <message-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("strip_handoff");
        json_t *msg = json_parse(argv[i], NULL);
        int is_obj = msg && msg->type == JSON_OBJECT;
        WCUP_BRANCH(is_obj);                                     /* B0 */
        if (!is_obj) {
            WCUP_VALSTR("none"); WCUP_VALSTR("0");
            WCUP_VALSTR("<unchanged>"); WCUP_BRANCH(0);
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
            WCUP_END("strip_handoff");
            continue;
        }
        json_t *content = json_obj_get(msg, "content");
        char *cls = cc_classify_summary_content(content);
        char clsbuf[32];
        snprintf(clsbuf, sizeof clsbuf, "%s", cls ? cls : "none");
        free(cls);
        WCUP_VALSTR(clsbuf);                                     /* V class */
        int meta = cc_has_compressed_summary_metadata(msg);
        char mb[2] = { meta ? '1' : '0', 0 };
        WCUP_VALSTR(mb);                                         /* V metadata */
        int is_summary = (strcmp(clsbuf, "none") != 0) || meta;
        WCUP_BRANCH(is_summary);                                 /* B1 */

        const char *in_content = (content && content->type == JSON_STRING)
                                     ? content->str_val : "";
        json_t *out = cc_strip_context_summary_handoff_message(msg);
        const char *out_label = "<unchanged>";
        json_t *oc = out ? json_obj_get(out, "content") : NULL;
        const char *oc_str = (out && oc && oc->type == JSON_STRING) ? oc->str_val : NULL;
        char diff;
        if (oc_str) {
            out_label = oc_str;
            diff = strcmp(oc_str, in_content) != 0;
        } else {
            diff = 0;
        }
        WCUP_VALSTR(out_label);                                  /* V result */
        WCUP_BRANCH(diff);                                       /* B2 */
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("strip_handoff");
    }
    return 0;
}
