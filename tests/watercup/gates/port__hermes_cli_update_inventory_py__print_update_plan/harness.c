/* gate: port__hermes_cli_update_inventory_py__print_update_plan — traced copy of print_update_plan (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern void print_update_plan(const upd_plan_t *plan);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s print_update_plan <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("print_update_plan");
    print_update_plan(argv[i]);
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("print_update_plan");
    }
    return 0;
}
