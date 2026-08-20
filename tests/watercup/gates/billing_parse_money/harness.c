/* gate: billing_parse_money — traced C copy of src/agent/billing_view.c::billing_parse_money.
 * Faithful port of LIVE Python agent/billing_view.py::parse_money.
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 *
 * Contract note: for VALID money strings (the agreed sub-contract) C and LIVE
 * agree. On INVALID input they diverge: C returns 0.0 (fail-closed numeric),
 * LIVE returns None (fail-open Optional[Decimal]). That divergence is documented,
 * not hidden — this gate exercises the agreed valid-input path.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "watercup.h"

static double billing_parse_money(const char *str) {
    WCUP_BEGIN("parse_money");
    if (!str) { WCUP_BRANCH(1); WCUP_VALSTR("null"); WCUP_VALSTR("0.0");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup); WCUP_END("parse_money"); return 0.0; }
    WCUP_BRANCH(0);
    while (*str == '$' || *str == ' ') str++;
    char dbuf[64];
    snprintf(dbuf, sizeof dbuf, "%.17g", atof(str));
    WCUP_VALSTR(dbuf);
    double v = atof(str);
    char vbuf[64];
    snprintf(vbuf, sizeof vbuf, "%.17g", v);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_money");
    return v;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_money <str>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) billing_parse_money(argv[i]);
    return 0;
}
