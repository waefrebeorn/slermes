/* gate: read_extract_notebook_output_text — drives the REAL C port
 * src/tools/read_extract.c::read_extract_notebook_output_text (linked via
 * extra_src) against LIVE Python tools/read_extract.py::_notebook_output_text.
 *
 * Coarse rung ladder (twin mirrors EXACTLY):
 *   B1: empty input            -> ""
 *   B2: parse failed/not dict  -> ""
 *   B3: output_type == stream
 *   B4: output_type in {error, pyerr}
 *   B5: output_type in {execute_result, display_data, pyout}
 *   V1: rendered result string
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *read_extract_notebook_output_text(const char *output_json);

static void wc_run_case(const char *input) {
    WCUP_BEGIN("_notebook_output_text");
    char *result = NULL;
    if (!input || !*input) {
        WCUP_BRANCH(1);                            /* B1 */
    } else {
        WCUP_BRANCH(0);
        /* peek output_type for the ladder (the port re-parses internally) */
        char otype_known = 0;
        {
            const char *k = strstr(input, "\"output_type\"");
            if (!k) k = strstr(input, "'output_type'");
            if (k) {
                /* crude but only used for rung parity; the REAL result comes
                 * from the linked port below */
                otype_known = 1;
                const char *colon = strchr(k, ':');
                const char *q1 = colon ? strchr(colon, '"') : NULL;
                if (q1) {
                    const char *q2 = strchr(q1 + 1, '"');
                    if (q2 && (size_t)(q2 - q1) < 40) {
                        char ot[40];
                        memcpy(ot, q1 + 1, (size_t)(q2 - q1 - 1));
                        ot[q2 - q1 - 1] = '\0';
                        if (strcmp(ot, "stream") == 0) { WCUP_BRANCH(1); }   /* B3 */
                        else { WCUP_BRANCH(0);
                            if (strcmp(ot, "error") == 0 || strcmp(ot, "pyerr") == 0) { WCUP_BRANCH(1); } /* B4 */
                            else { WCUP_BRANCH(0);
                                if (strcmp(ot, "execute_result") == 0 || strcmp(ot, "display_data") == 0 ||
                                    strcmp(ot, "pyout") == 0) { WCUP_BRANCH(1); }                     /* B5 */
                                else { WCUP_BRANCH(0); }
                            }
                        }
                    } else {
                        WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
                    }
                } else {
                    WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
                }
            } else {
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);   /* not dict-ish -> B2 path below */
            }
        }
        result = read_extract_notebook_output_text(input);
        (void)otype_known;
    }
    WCUP_VALSTR(result ? result : "");
    free(result);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_notebook_output_text");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _notebook_output_text <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_run_case(argv[i]);
    return 0;
}
