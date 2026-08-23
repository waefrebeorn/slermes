/* gate: fuzzy_match_detect_escape_drift — traced C copy of
 * src/hermes_cli/fuzzy_match_helpers.c::fuzzy_match_detect_escape_drift.
 * PoP of LIVE tools/fuzzy_match.py::_detect_escape_drift (quote-suspect
 * pre-check -> matched regions -> per-suspect rungs).
 *
 * Inputs are "old_string|new_string|content"; matched region = whole content.
 * Only the ERR/NONE classification is value-traced (the error MESSAGE text
 * differs between the ports by design).
 *
 * KNOWN DIVERGENCE (inputs avoid it): LIVE's pre-check falls through to its
 * backslash-doubling stage when old_string has backslashes without quote
 * suspects; the C port returns NULL early and lacks that stage.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static void detect_escape_drift(const char *input) {
    WCUP_BEGIN("_detect_escape_drift");
    char old_s[128] = "", new_s[128] = "", content[192] = "";
    if (input) {
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", input);
        char *p1 = strchr(tmp, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (p1 && p2) {
            *p1 = '\0'; *p2 = '\0';
            snprintf(old_s, sizeof old_s, "%s", tmp);
            snprintf(new_s, sizeof new_s, "%s", p1 + 1);
            snprintf(content, sizeof content, "%s", p2 + 1);
        }
    }
    WCUP_VALSTR(old_s);
    WCUP_VALSTR(new_s);
    WCUP_VALSTR(content);

    bool err = false;
    /* C: early-out unless new_string contains a quote-suspect escape. */
    bool has_suspects = strstr(new_s, "\\'") != NULL || strstr(new_s, "\\\"") != NULL;
    WCUP_BRANCH(has_suspects);
    if (has_suspects) {
        /* matched region = whole content (one match). */
        WCUP_LOOP_TICK();
        const char *region = content;
        const char *suspects[2] = {"\\'", "\\\""};
        for (int k = 0; k < 2; k++) {
            if (err) break;
            bool hit = strstr(new_s, suspects[k]) && strstr(old_s, suspects[k]) &&
                       !strstr(region, suspects[k]);
            WCUP_BRANCH(hit);
            if (hit) err = true;
        }
    }
    WCUP_BRANCH(err);
    char rbuf[8];
    snprintf(rbuf, sizeof rbuf, "%s", err ? "ERR" : "NONE");
    WCUP_VALSTR(rbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_detect_escape_drift");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _detect_escape_drift <old|new|content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) detect_escape_drift(argv[i]);
    return 0;
}
