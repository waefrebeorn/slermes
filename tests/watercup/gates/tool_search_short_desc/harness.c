/* gate: tool_search_short_desc — links the REAL C port
 * src/tools/tool_search.c::short_desc (post-parity-fix). LIVE twin:
 * tools/tool_search.py::_short_desc.
 *
 * Input argv: "<max_chars>\t<description>" (or bare description for the
 * 60 default; literal \\n / \\t escapes decoded first).
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: empty after whitespace collapse   B2: sentence terminator found
 *   B3: over max_chars                    B4: space in clip window
 *   VSTR(final description)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"

void short_desc(const char *description, int max_chars, char *out, size_t out_size);

/* Out-of-path external (registry lookup path only). */
void *registry_find(const char *name) { (void)name; return NULL; }

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && (p[1] == 't' || p[1] == 'n')) {
            *o++ = p[1] == 't' ? '\t' : '\n'; p++;
        } else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);

    int max_chars = 60;
    const char *desc = buf;
    char *tab = strchr(buf, '\t');
    if (tab) {
        *tab = '\0';
        char *end;
        long v = strtol(buf, &end, 10);
        if (*buf && !*end) { max_chars = (int)v; desc = tab + 1; }
        else { *tab = '\t'; desc = buf; }
    }

    /* mirror LIVE's collapse for tracing */
    char text[2048];
    size_t tl = 0;
    const char *pp = desc;
    while (*pp) {
        if (isspace((unsigned char)*pp)) {
            if (tl > 0 && text[tl-1] != ' ') text[tl++] = ' ';
            while (*pp && isspace((unsigned char)*pp)) pp++;
            continue;
        }
        text[tl++] = *pp++;
    }
    text[tl] = '\0';
    if (tl > 0 && text[tl - 1] == ' ') text[--tl] = '\0';  /* LIVE: no trailing space */
    bool empty = tl == 0;
    /* sentence terminator search on collapsed text */
    bool term = false, keep_dot = false;
    size_t cut = tl;
    for (size_t i = 0; i < tl; i++) {
        if (text[i] == '.' || text[i] == '!' || text[i] == '?' || text[i] == '\n') {
            term = true;
            keep_dot = text[i] == '.';
            cut = i + (keep_dot ? 1 : 0);
            break;
        }
    }
    size_t eff_len = term ? cut : tl;
    bool over = !empty && eff_len > (size_t)max_chars;
    char clipped[1024];
    size_t cl2 = eff_len < (size_t)max_chars ? eff_len : (size_t)max_chars;
    memcpy(clipped, text, cl2); clipped[cl2] = '\0';

    WCUP_BEGIN("_short_desc");
    WCUP_BRANCH(empty);                             /* B1 */
    WCUP_BRANCH(term);                              /* B2 */
    WCUP_BRANCH(over);                              /* B3 */
    if (over)
        WCUP_BRANCH(strchr(clipped, ' ') != NULL);  /* B4 */

    char out[2048];
    short_desc(desc, max_chars, out, sizeof out);
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_short_desc");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> \"<max>\\tdesc\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
