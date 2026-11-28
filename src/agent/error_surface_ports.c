/* Standalone C home for error_surface ports. src/agent/error_surface.c cannot
 * compile (it includes error_classifier.h, which does not exist in the tree), so
 * ports live here and gates compile this file via extra_src. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: agent_error_surface_u_looks_like_stream_drop @ agent/error_surface.py:_looks_like_stream_drop */
/* any(fragment in message.lower() for fragment in _STREAM_DROP_FRAGMENTS) -- 7
 * fragments, substring test on the ASCII-lowercased message. */
static const char *ES_FRAGS[] = {"stream connection","peer closed connection","incomplete chunked read",
    "connection broken","stream ended prematurely","sse","mid-stream"};
int agent_error_surface_u_looks_like_stream_drop(const char *arg) {
    const char *m = arg ? arg : "";
    char *low = malloc(strlen(m) + 1); size_t w = 0;
    for (const char *p = m; *p; p++) { char c = *p; low[w++] = (c>='A'&&c<='Z') ? (char)(c+32) : c; }
    low[w] = 0;
    int hit = 0;
    for (int i = 0; i < (int)(sizeof(ES_FRAGS)/sizeof(ES_FRAGS[0])) && !hit; i++)
        if (strstr(low, ES_FRAGS[i])) hit = 1;
    free(low);
    return hit;
}
