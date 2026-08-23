/* gate: gateway_helpers_md_chunk_markdown_paragraphs — LINKS the REAL C port
 * src/gateway/platforms/platforms_helpers_md.c::helpers_md_chunk_markdown_paragraphs
 * (PoP: gateway/platforms/helpers.py:_chunk_markdown_paragraphs) via extra_src.
 * Input encoding: max_chars\x1ftext. Value: chunks joined with \x1f; trace:
 * short-circuit branch + one loop tick / value per output chunk. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "platforms_helpers_md.h"

static void run_one(size_t max_chars, const char *text) {
    WCUP_BEGIN("chunk_markdown_paragraphs");
    size_t total = text ? strlen(text) : 0;
    if (!text || total <= max_chars) {
        WCUP_BRANCH(1);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("chunk_markdown_paragraphs");
        return;
    }
    WCUP_BRANCH(0);
    char **chunks = helpers_md_chunk_markdown_paragraphs(text, max_chars);
    for (char **p = chunks; p && *p; p++) {
        WCUP_LOOP_TICK();
        WCUP_VALSTR(*p);
    }
    helpers_md_free_strlist(chunks);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("chunk_markdown_paragraphs");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s chunk_markdown_paragraphs <max\\x1ftext>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *sep = strchr(s, '\x1f');
        size_t mx = 200;
        const char *text = "";
        if (sep) { *sep = '\0'; mx = (size_t)atol(s); text = sep + 1; }
        else text = s;
        run_one(mx, text);
        free(s);
    }
    return 0;
}
