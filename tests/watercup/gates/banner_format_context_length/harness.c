/* gate: banner_format_context_length — traced copy of
 * src/hermes_cli/banner_pure.c::banner_format_context_length
 * (LIVE twin: hermes_cli/banner.py::_format_context_length).
 *
 * Rung order (must match twin.py EXACTLY):
 *   B1: tokens >= 1_000_000   B2: near-integer M -> "%dM" else ".1fM"
 *   B3: tokens >= 1_000       B4: near-integer K -> "%dK" else ".1fK"
 *   else plain decimal.  V: formatted result.
 *
 * NOTE: Python round() is ROUND_HALF_EVEN (banker's); the port must
 * reproduce that or exact-half probes carve a different cup.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watercup.h"

/* Python round(): half to even */
static double py_round(double v) {
    double f = floor(v);
    double diff = v - f;
    if (diff > 0.5) return f + 1;
    if (diff < 0.5) return f;
    return (fmod(f, 2.0) == 0.0) ? f : f + 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fn <tokens>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        long long tokens = atoll(argv[i]);
        char out[64];
        WCUP_BEGIN("banner_format_context_length");
        if (tokens >= 1000000) {                              /* B1 */
            WCUP_BRANCH(1);
            double val = tokens / 1000000.0;
            double rounded = py_round(val);
            if (fabs(val - rounded) < 0.05) {                 /* B2 */
                WCUP_BRANCH(1);
                snprintf(out, sizeof out, "%.0fM", rounded);
            } else {
                WCUP_BRANCH(0);
                snprintf(out, sizeof out, "%.1fM", val);
            }
        } else if (tokens >= 1000) {                          /* B3 */
            WCUP_BRANCH(1);
            double val = tokens / 1000.0;
            double rounded = py_round(val);
            if (fabs(val - rounded) < 0.05) {                 /* B4 */
                WCUP_BRANCH(1);
                snprintf(out, sizeof out, "%.0fK", rounded);
            } else {
                WCUP_BRANCH(0);
                snprintf(out, sizeof out, "%.1fK", val);
            }
        } else {
            WCUP_BRANCH(0);
            snprintf(out, sizeof out, "%lld", tokens);
        }
        WCUP_VALSTR(out);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("banner_format_context_length");
    }
    return 0;
}
