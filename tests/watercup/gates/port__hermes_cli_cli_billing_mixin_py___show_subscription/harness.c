/* gate: port__hermes_cli_cli_billing_mixin_py___show_subscription — GAP sentinel for _show_subscription */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_show_subscription";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _show_subscription <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_show_subscription");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_show_subscription");
    }
    return 0;
}
