/* tests/watercup/demo_c.c — C side of the water-cup demo.
 *
 * Faithful port of usage_pricing.format_cost_label WITH the watercup tracer
 * macros. All traced values use WCUP_VALSTR with a CANONICAL string encoding
 * (double -> "%.17g", bool -> "True"/"False") so the C cup matches the
 * Python Cup.valstr() cup byte-for-byte. Prints one cup hash per argv input.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

/* canonical encodings shared with watercup_py.Cup.valstr */
static void trace_double(wcup_t *c, double v) {
    char b[64];
    snprintf(b, sizeof b, "%.17g", v);
    wcup_valstr(c, b);
}
static void trace_bool(wcup_t *c, int v) {
    wcup_valstr(c, v ? "True" : "False");
}

static const char *fmt_est_cost(double amount, char *out, int outsz) {
    WCUP_BEGIN("fmt_est_cost");
    trace_double(&_wc, amount);
    if (amount == 0.0) {
        WCUP_BRANCH(1);
        snprintf(out, outsz, "$0.00");
    } else {
        WCUP_BRANCH(0);
        trace_bool(&_wc, amount < 0.01);
        if (amount < 0.01) {
            WCUP_BRANCH(1);
            char s[32];
            snprintf(s, sizeof s, "~$%.4f", amount);
            WCUP_VALSTR(s);
            if (strcmp(s, "~$0.0000") == 0) {
                WCUP_BRANCH(1);
                snprintf(out, outsz, "~$<0.0001");
            } else {
                WCUP_BRANCH(0);
                snprintf(out, outsz, "%s", s);
            }
        } else {
            WCUP_BRANCH(0);
            trace_double(&_wc, amount);
            snprintf(out, outsz, "~$%.2f", amount);
        }
    }
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("fmt_est_cost");
    return out;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        double x = atof(argv[i]);
        char out[64];
        fmt_est_cost(x, out, sizeof out);
    }
    return 0;
}
