/* gate: port__hermes_cli_update_receipt_py__record_skip — traced copy of record_skip (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern void record_skip(const char *name, const char *reason);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_skip <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("record_skip");
    record_skip(argv[i], argv[i]);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("record_skip");
    }
    return 0;
}
