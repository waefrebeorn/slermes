/* gate: tools_tirith_security_py___record_tirith_crash — traced copy of tools_tirith_security_u_record_tirith_crash */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_tirith_security_u_record_tirith_crash(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_tirith_security_u_record_tirith_crash");
    int result = tools_tirith_security_u_record_tirith_crash(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_tirith_security_u_record_tirith_crash");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_tirith_security_u_record_tirith_crash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
