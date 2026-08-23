/* gate: session_search_tool_normalize_title_query — traced copy of
 * src/tools/session_search_tool.c::normalize_title_query.
 * LIVE twin: tools/session_search_tool.py::_normalize_title_query.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: null query -> ""
 *   LOOP lead whitespace ticks
 *   LOOP trail whitespace ticks
 *   LOOP leading quote-strip ticks
 *   LOOP trailing quote-strip ticks
 *   B2: nonempty result
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static char *wc_normalize_title_query(const char *query) {
    WCUP_BEGIN("normalize_title_query");
    char *out;
    if (!query) {                                   /* B1 */
        WCUP_BRANCH(1);
        out = strdup("");
        goto done;
    }
    WCUP_BRANCH(0);

    const char *start = query;
    while (*start && isspace((unsigned char)*start)) { WCUP_LOOP_TICK(); start++; }

    const char *end = query + strlen(query);
    while (end > start && isspace((unsigned char)*(end - 1))) { WCUP_LOOP_TICK(); end--; }

    const char *lo = start;
    const char *hi = end;
    while (lo < hi && (*lo == '`' || *lo == '\'' || *lo == '"')) { WCUP_LOOP_TICK(); lo++; }
    while (hi > lo && (hi[-1] == '`' || hi[-1] == '\'' || hi[-1] == '"')) { WCUP_LOOP_TICK(); hi--; }

    size_t len = (size_t)(hi - lo);
    out = malloc(len + 1);
    if (len > 0) memcpy(out, lo, len);
    out[len] = '\0';
    WCUP_BRANCH(len > 0);                           /* B2 */
done: {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_title_query");
        return out;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_title_query <query>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_normalize_title_query(argv[i]);
    return 0;
}
