/* gate: hermes_cli_config_py__set_config_value — traced copy of config_set_config_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int config_set_config_value(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("config_set_config_value");
    int result = config_set_config_value(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("config_set_config_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s config_set_config_value <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
