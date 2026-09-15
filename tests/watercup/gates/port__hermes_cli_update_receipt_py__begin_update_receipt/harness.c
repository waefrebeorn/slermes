/* gate: port__hermes_cli_update_receipt_py__begin_update_receipt — traced copy of begin_update_receipt (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern void begin_update_receipt(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s begin_update_receipt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("begin_update_receipt");
    begin_update_receipt(argv[i]);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("begin_update_receipt");
    }
    return 0;
}
