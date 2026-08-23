/* gate: mcp_tool_safe_numeric — traced copy of
 * src/tools/mcp_tool.c::mcp_tool_safe_numeric.
 * LIVE twin: tools/mcp_tool.py::_safe_numeric (coerce=int, minimum=1).
 *
 * Input encoding (single argv token per case): "<value|NULL>\t<default>"
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: null value -> default
 *   parse; B2: unparseable -> default
 *   (result)
 * Inputs are restricted to values where C's strtod and LIVE int() agree and
 * the result is >= the minimum clamp of 1 (see gate.json notes).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static double wc_safe_numeric(const char *str, double def) {
    WCUP_BEGIN("safe_numeric");
    double result;
    if (!str) {                                      /* B1 */
        WCUP_BRANCH(1);
        result = def;
        goto done;
    }
    WCUP_BRANCH(0);
    {
        char *endptr = NULL;
        double val = strtod(str, &endptr);
        if (endptr == str) {                         /* B2 */
            WCUP_BRANCH(1);
            result = def;
        } else {
            WCUP_BRANCH(0);
            result = val;
        }
    }
done: {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("safe_numeric");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s safe_numeric <value>\\t<default>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *t1 = strchr(arg, '\t');
        if (!t1) { fprintf(stderr, "bad input: %s\n", argv[i]); free(arg); continue; }
        *t1 = '\0';
        wc_safe_numeric(strcmp(arg, "NULL") == 0 ? NULL : arg, atof(t1 + 1));
        free(arg);
    }
    return 0;
}
