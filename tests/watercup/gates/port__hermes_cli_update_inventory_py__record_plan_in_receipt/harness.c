/* gate: port__hermes_cli_update_inventory_py__record_plan_in_receipt — traced copy of record_plan_in_receipt (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern void record_plan_in_receipt(const upd_plan_t *plan);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_plan_in_receipt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("record_plan_in_receipt");
    record_plan_in_receipt(argv[i]);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("record_plan_in_receipt");
    }
    return 0;
}
