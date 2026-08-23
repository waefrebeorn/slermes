/* gate: dashboard_auth_prefix_normalise_public_url — traced copy of
 * src/hermes_cli/dashboard_auth_prefix.c::cli_hermes_cli_dashboard_auth_prefix__normalise_public_url
 * (LIVE twin: hermes_cli/dashboard_auth/prefix.py::_normalise_public_url).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty/absent raw        -> ""
 *   B2: whitespace-only         -> ""
 *   B3: contains reject chars   -> ""
 *   B4: scheme is http/https    (else "")
 *   V1: normalised value
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static void wc_normalise(const char *raw) {
    WCUP_BEGIN("normalise_public_url");
    char out[512];
    out[0] = '\0';
    bool ok = false;
    const char *p = raw ? raw : "";
    if (!p[0]) {                                   /* B1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        const char *q = p;
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) {                                 /* B2 */
            WCUP_BRANCH(1);
            goto emit;
        }
        WCUP_BRANCH(0);
        p = q;
    }
    {
        bool rejected = false;
        for (const char *r = p; *r; r++) {
            if (*r == '"' || *r == '\'' || *r == '<' || *r == '>' ||
                *r == ' ' || *r == '\n' || *r == '\r' || *r == '\t') {
                rejected = true;
                break;
            }
        }
        if (rejected) {                            /* B3 */
            WCUP_BRANCH(1);
            goto emit;
        }
        WCUP_BRANCH(0);
    }
    {
        bool scheme_ok = (strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0);
        if (!scheme_ok) {                          /* B4 */
            WCUP_BRANCH(0);
            goto emit;
        }
        WCUP_BRANCH(1);
        snprintf(out, sizeof out, "%s", p);
        size_t len = strlen(out);
        while (len > 0 && out[len - 1] == '/') out[--len] = '\0';
        ok = true;
    }
emit:
    WCUP_VALSTR(out);                              /* V1 */
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalise_public_url");
        (void)ok;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalise_public_url <raw>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_normalise(argv[i]);
    return 0;
}
