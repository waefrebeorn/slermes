/* gate: base_extract_media — traced copy of
 * src/gateway/platforms/base_ext2.c::gw_extract_media (POSIX-regex
 * "MEDIA:\s*([^\s`\"'\n]+)" scan, per-match is_voice probe).
 * PoP of LIVE gateway/platforms/base.py::extract_media (media-list rung).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty content -> no scan
 *   LOOP over regex matches:
 *     B2: captured path non-empty
 *     V : path string
 *     B3: is_voice flag for this entry
 *
 * KNOWN DIVERGENCES (documented; inputs are curated to avoid them):
 *  - PORT BUG in base_ext2.c: the POSIX bracket class [^\s`"'\n] treats
 *    backslash escapes as LITERAL characters, so the capture set excludes
 *    's'/'S' and 'n'/'N' (stopping mid-path, e.g. "/q/pia" for piano.wav)
 *    while ALLOWING spaces and newlines (capturing trailing junk like
 *    " tail"). LIVE's Python regex handles all of these correctly. Inputs
 *    therefore avoid paths containing s/n and tags followed by spaces.
 *  - LIVE dedupes on expanded path (first wins); the C port appends every
 *    match -> duplicate-path inputs excluded.
 *  - LIVE masks protected spans (code blocks/quotes, JSON string values)
 *    before scanning -> backtick-wrapped example tags excluded.
 *  - LIVE gates is_voice on audio extensions; the C port flags every entry
 *    when "[[audio_as_voice]]" appears in the remaining text -> only single-
 *    audio-entry voice inputs are exercised.
 *  Quote/backtick-separated tag runs are used to exercise multi-match loops
 *  (the excluded quote chars terminate the C capture exactly like LIVE's
 *  path normalization strips them).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include "watercup.h"

static void extract_media(const char *content) {
    WCUP_BEGIN("extract_media");
    if (!content || !*content) {            /* B1 */
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        regex_t regex;
        if (regcomp(&regex, "MEDIA:\\s*([^\\s`\"'\\n]+)", REG_EXTENDED | REG_ICASE) == 0) {
            regmatch_t matches[2];
            const char *p = content;
            while (regexec(&regex, p, 2, matches, 0) == 0) {
                WCUP_LOOP_TICK();
                size_t path_len = matches[1].rm_eo - matches[1].rm_so;
                bool path_ok = path_len > 0;         /* B2 */
                WCUP_BRANCH(path_ok);
                char path[1024];
                if (path_ok) {
                    memcpy(path, p + matches[1].rm_so, path_len);
                    path[path_len] = '\0';
                    WCUP_VALSTR(path);
                }
                bool is_voice = (strstr(p, "[[audio_as_voice]]") != NULL); /* B3 */
                WCUP_BRANCH(is_voice);
                p += matches[0].rm_eo;
            }
            regfree(&regex);
        }
    }
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("extract_media");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_media <content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) extract_media(argv[i]);
    return 0;
}
