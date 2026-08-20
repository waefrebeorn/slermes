/* gate: billing_format_money — traced C copy of
 * src/agent/billing_view.c::billing_format_money.
 * Faithful port of LIVE Python agent/billing_view.py:format_money.
 * WCUP_* records the SAME control structure LIVE takes, so the cup matches iff
 * the C port executes the identical branch ladder as LIVE Python.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "watercup.h"

static void billing_format_money(double amount, char *buf, size_t bufsz) {
    WCUP_BEGIN("format_money");
    char dbuf[64];
    snprintf(dbuf, sizeof dbuf, "%.17g", amount);
    WCUP_VALSTR(dbuf);
    if (amount == (long long)amount) {
        WCUP_BRANCH(1);   /* whole dollars -> no decimals */
        snprintf(buf, bufsz, "$%.0f", amount);
    } else {
        WCUP_BRANCH(0);   /* fractional -> 2dp */
        snprintf(buf, bufsz, "$%.2f", amount);
    }
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("format_money");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_money <amount>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[64];
        billing_format_money(atof(argv[i]), buf, sizeof buf);
    }
    return 0;
}
