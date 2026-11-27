/* gate: clarify timeout resolution — the number, not a log line.
 *
 * Python: clarify.timeout if explicitly set (and not null), else
 * agent.clarify_timeout, else 90; <=0 kept verbatim; non-numeric -> 90.
 * The port printed the answer, returned 0, defaulted to 3600 and echoed "abc".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_clarify_gateway_resolve_clarify_timeout(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_clarify_timeout <config-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[64];
        WCUP_BEGIN("wc_clarify_timeout");
        WCUP_BRANCH(argv[i][0] != '\0');                 /* B1: config given */
        int t = tools_clarify_gateway_resolve_clarify_timeout(argv[i]);
        snprintf(buf, sizeof buf, "%d", t);
        WCUP_VALSTR(buf);                                /* V1: the resolved seconds */
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wc_clarify_timeout");
    }
    return 0;
}
