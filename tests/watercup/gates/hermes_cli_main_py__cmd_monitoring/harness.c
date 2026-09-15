/* gate: hermes_cli_main_py__cmd_monitoring — traced copy of hermes_cli_main_cmd_monitoring */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_main_cmd_monitoring(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("hermes_cli_main_cmd_monitoring");
    int result = hermes_cli_main_cmd_monitoring(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_main_cmd_monitoring");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_main_cmd_monitoring <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
