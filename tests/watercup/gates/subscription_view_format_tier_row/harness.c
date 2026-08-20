/* gate: subscription_view_format_tier_row — traced C copy of the control
 * structure of src/agent/ports_wrappers.c::agent_subscription_view_format_tier_row.
 * Faithful port of Python agent/subscription_view.py:format_tier_row.
 * Arg is "name<TAB>dollars<TAB>credits". The cup traces: null/empty guard,
 * parse success, whole-vs-fractional dollar branch, credits>0 suffix branch.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static void format_tier_row(const char *arg) {
    WCUP_BEGIN("format_tier_row");
    WCUP_VALSTR(arg ? arg : "(null)");
    if (!arg || !*arg) { WCUP_BRANCH(0); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("format_tier_row"); return; }
    WCUP_BRANCH(1);
    char name[128];
    double dollars = 0, credits = -1;
    if (sscanf(arg, "%127[^\t]\t%lf\t%lf", name, &dollars, &credits) < 2) {
        WCUP_BRANCH(0); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("format_tier_row"); return;
    }
    WCUP_BRANCH(1);
    /* whole vs fractional dollars determines the money-format branch */
    if (dollars == (long long)dollars) {
        WCUP_BRANCH(1);          /* whole dollars -> $%lld grouped */
    } else {
        WCUP_BRANCH(0);          /* fractional -> $%.2f grouped */
    }
    /* credits suffix only when present and > 0 */
    if (credits > 0) {
        WCUP_BRANCH(1);
        WCUP_VALSTR("has-credits");
    } else {
        WCUP_BRANCH(0);
        WCUP_VALSTR("no-credits");
    }
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("format_tier_row");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_tier_row <arg>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) format_tier_row(argv[i]);
    return 0;
}
