/* gate: threatpatterns_first_match — drives the REAL
 * lib/libthreatpatterns/threat_patterns.c::threat_patterns_check against the
 * LIVE tools/threat_patterns.py::scan_for_threats first-match semantics.
 *
 * Input tag (argv): "tp:<scope>|<content>"  with scope ∈ all|context|strict.
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: a pattern matched at this scope
 *   VSTR(matched pattern_id)   — only emitted on a match
 *
 * Note: the LIVE scanner additionally does NFKC normalisation and an
 * invisible-unicode pass before the regex sweep; those passes have no C port
 * counterpart, so gate inputs stay ASCII and non-matching for them.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

/* Use the REAL lib header — the harness previously declared its own struct
 * without the leading `bool matched`, so pattern_id was read one byte off
 * (cup hashed "\x01prompt_injection" vs Python "prompt_injection"). */
#include "../../lib/libthreatpatterns/threat_patterns.h"
extern int  threat_patterns_init(void);
extern bool threat_patterns_check(const char *content, int scope_mask,
                                  threat_match_t *match_out);
#define WC_SCOPE_ALL     0
#define WC_SCOPE_CONTEXT 1
#define WC_SCOPE_STRICT  2

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    threat_patterns_init();

    for (int i = 2; i < argc; i++) {
        const char *in = argv[i];
        if (strncmp(in, "tp:", 3) != 0) { fprintf(stderr, "bad input\n"); return 2; }
        const char *p = in + 3;
        char scope[16];
        const char *bar = strchr(p, '|');
        snprintf(scope, sizeof scope, "%.*s", (int)(bar - p), p);
        int mask;
        if (strcmp(scope, "all") == 0)
            mask = 1 << WC_SCOPE_ALL;
        else if (strcmp(scope, "context") == 0)
            mask = (1 << WC_SCOPE_ALL) | (1 << WC_SCOPE_CONTEXT);
        else
            mask = (1 << WC_SCOPE_ALL) | (1 << WC_SCOPE_CONTEXT) |
                   (1 << WC_SCOPE_STRICT);

        WCUP_BEGIN("threatpatterns_first_match");
        threat_match_t m;
        bool matched = threat_patterns_check(bar + 1, mask, &m);
        WCUP_BRANCH(matched);
        if (matched)
            WCUP_VALSTR(m.pattern_id);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("threatpatterns_first_match");
    }
    return 0;
}
