/* gate: port__hermes_cli_doctor_py___apply_doctor_tool_availability_overrides — traced copy of doctor_apply_tool_availability_overrides (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool doctor_apply_tool_availability_overrides(const char *provider);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_apply_tool_availability_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("doctor_apply_tool_availability_overrides");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = doctor_apply_tool_availability_overrides(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("doctor_apply_tool_availability_overrides");
    }
    return 0;
}
