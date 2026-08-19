/* gate: usage_pricing_format_cost_label — traced C copy of
 * src/agent/usage_pricing.c::usage_pricing_format_cost_label.
 * Faithful port of Python agent/usage_pricing.py:format_cost_label.
 * The WCUP_* calls record the SAME control structure the live function takes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static const char *format_cost_label(double amount, char *buf, size_t bufsz) {
    WCUP_BEGIN("format_cost_label");
    char dbuf[64];
    snprintf(dbuf, sizeof dbuf, "%.17g", amount);
    WCUP_VALSTR(dbuf);
    if (amount == 0.0) {
        WCUP_BRANCH(1);
        snprintf(buf, bufsz, "$0.00");
    } else {
        WCUP_BRANCH(0);
        if (amount < 0.01) {
            WCUP_BRANCH(1);
            snprintf(buf, bufsz, "~$%.4f", amount);
            if (strcmp(buf, "~$0.0000") == 0) {
                WCUP_BRANCH(1);
                snprintf(buf, bufsz, "~$<0.0001");
            } else {
                WCUP_BRANCH(0);
            }
        } else {
            WCUP_BRANCH(0);
            snprintf(buf, bufsz, "~$%.2f", amount);
        }
    }
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("format_cost_label");
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_cost_label <amount>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[64];
        format_cost_label(atof(argv[i]), buf, sizeof buf);
    }
    return 0;
}
