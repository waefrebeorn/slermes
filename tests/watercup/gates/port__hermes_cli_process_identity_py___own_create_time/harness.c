/* gate: port__hermes_cli_process_identity_py___own_create_time — traced copy of process_identity_own_create_time (double) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern double process_identity_own_create_time(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s process_identity_own_create_time <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("process_identity_own_create_time");
        double result = process_identity_own_create_time(argv[i]);
            char buf[64];\n    snprintf(buf, sizeof(buf), "%p", (void*)&result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("process_identity_own_create_time");
    }
    return 0;
}
