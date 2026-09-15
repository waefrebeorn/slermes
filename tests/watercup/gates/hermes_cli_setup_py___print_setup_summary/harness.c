/* gate: hermes_cli_setup_py___print_setup_summary — traced copy of stp_print_setup_summary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int stp_print_setup_summary(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("stp_print_setup_summary");
    int result = stp_print_setup_summary(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stp_print_setup_summary");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_print_setup_summary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
