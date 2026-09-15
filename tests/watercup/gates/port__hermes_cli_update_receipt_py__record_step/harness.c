/* gate: port__hermes_cli_update_receipt_py__record_step — traced copy of record_step (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern void record_step(const char *name, int ok, const char *detail);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_step <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("record_step");
    record_step(argv[i], argv[i], argv[i]);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("record_step");
    }
    return 0;
}
