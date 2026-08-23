/* gate: context_compressor_summarize_tool_result_unguarded — drives the REAL
 * src/agent/context_compressor_pure.c::cc_summarize_tool_result_unguarded
 * (linked via extra_src). LIVE twin:
 * agent/context_compressor.py::_summarize_tool_result_unguarded.
 *
 * Inputs: "<tool_name>\t<tool_args-json-or-->\t<tool_content>" per cup.
 * Rung order (must match twin.py EXACTLY):
 *   B1 terminal / V summary line
 *   B2 read_file / V
 *   B3 write_file / V
 *   B4 search_files / V
 *   else fallback / V
 *
 * AGREED SUB-CONTRACT (gate.json notes): only tool names implemented by the
 * C port; content_len < 1000 (LIVE uses {,} thousands separators); content
 * not whitespace-only (line_count guard differs); string-only command args.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *cc_summarize_tool_result_unguarded(const char *tool_name,
                                         const char *tool_args,
                                         const char *tool_content);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s summarize <name\targs\tcontent>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        /* split argv on tabs (2 splits max) */
        char buf[4096];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *args_s = strchr(buf, '\t');
        if (args_s) { *args_s = '\0'; args_s++; }
        char *content_s = args_s ? strchr(args_s, '\t') : NULL;
        if (content_s) { *content_s = '\0'; content_s++; }
        WCUP_BEGIN("summarize_tool_result_unguarded");
        int b1 = strcmp(buf, "terminal") == 0;
        WCUP_BRANCH(b1);                           /* B1 */
        int b2 = !b1 && strcmp(buf, "read_file") == 0;
        WCUP_BRANCH(b2);                           /* B2 */
        int b3 = !b1 && !b2 && strcmp(buf, "write_file") == 0;
        WCUP_BRANCH(b3);                           /* B3 */
        int b4 = !b1 && !b2 && !b3 && strcmp(buf, "search_files") == 0;
        WCUP_BRANCH(b4);                           /* B4 */
        char *out = cc_summarize_tool_result_unguarded(
            buf,
            (args_s && strcmp(args_s, "-") != 0) ? args_s : "",
            content_s ? content_s : "");
        WCUP_VALSTR(out ? out : "");               /* V */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("summarize_tool_result_unguarded");
        free(out);
    }
    return 0;
}
