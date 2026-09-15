/* gate: port__hermes_cli_update_receipt_py__print_fleet_version_matrix — traced copy of print_fleet_version_matrix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int print_fleet_version_matrix(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("print_fleet_version_matrix");
    int result = print_fleet_version_matrix(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("print_fleet_version_matrix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s print_fleet_version_matrix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
