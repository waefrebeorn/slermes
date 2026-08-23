/* gate: think_scrubber_strip_orphan_close_tags — traced copy of
 * src/agent/think_scrubber.c::strip_orphan
 * (LIVE twin: agent/think_scrubber.py::StreamingThinkScrubber._strip_orphan_close_tags).
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   per char:
 *     B1: text[i..i+1] == "</"
 *       per tag (only when i+tag_len <= len):
 *         B2: tag matches at i
 *       B3: matched -> skip tag + trailing ws
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

#define MAX_TAG_VARIANTS 5
#define MAX_TAG_LEN      24

static const char *CLOSE_TAGS[MAX_TAG_VARIANTS] = {
    "</think>", "</thinking>", "</reasoning>", "</thought>",
    "</REASONING_SCRATCHPAD>"
};
static char CLOSE_TAGS_LOWER[MAX_TAG_VARIANTS][MAX_TAG_LEN];
static int TAG_CLOSE_LENS[MAX_TAG_VARIANTS];
static int g_tags_init = 0;

static void init_tags(void) {
    if (g_tags_init) return;
    for (int i = 0; i < MAX_TAG_VARIANTS; i++) {
        int clen = (int)strlen(CLOSE_TAGS[i]);
        TAG_CLOSE_LENS[i] = clen;
        for (int j = 0; j < clen && j < MAX_TAG_LEN - 1; j++)
            CLOSE_TAGS_LOWER[i][j] = (char)tolower((unsigned char)CLOSE_TAGS[i][j]);
        CLOSE_TAGS_LOWER[i][clen] = '\0';
    }
    g_tags_init = 1;
}

static int strip_orphan(const char *text, int text_len, char *out, int cap) {
    WCUP_BEGIN("strip_orphan");
    int opos = 0;
    for (int i = 0; i < text_len; ) {
        /* B1: close-tag start? */
        if (i + 1 < text_len && text[i] == '<' && text[i+1] == '/') {
            WCUP_BRANCH(1);
            int hit = 0;
            for (int t = 0; t < MAX_TAG_VARIANTS; t++) {
                int tl = TAG_CLOSE_LENS[t];
                if (i + tl > text_len) continue;   /* no event (mirrored in twin) */
                int m = 1;
                for (int j = 0; j < tl; j++)
                    if (tolower((unsigned char)text[i+j]) !=
                        (unsigned char)CLOSE_TAGS_LOWER[t][j]) { m = 0; break; }
                WCUP_BRANCH(m);
                if (m) {
                    i += tl;
                    while (i < text_len && (text[i]==' '||text[i]=='\t'||text[i]=='\n'||text[i]=='\r')) i++;
                    hit = 1; break;
                }
            }
            if (hit) { WCUP_BRANCH(1); continue; }
            WCUP_BRANCH(0);
        } else {
            WCUP_BRANCH(0);
        }
        if (opos < cap - 1) out[opos++] = text[i];
        i++;
    }
    out[opos] = '\0';
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_orphan");
    return opos;
}

int main(int argc, char **argv) {
    init_tags();
    if (argc < 3) { fprintf(stderr, "usage: %s strip_orphan <text>...\n", argv[0]); return 2; }
    static char out[65536];
    for (int i = 2; i < argc; i++) {
        memset(out, 0, sizeof out);
        strip_orphan(argv[i], (int)strlen(argv[i]), out, (int)sizeof out);
    }
    return 0;
}
