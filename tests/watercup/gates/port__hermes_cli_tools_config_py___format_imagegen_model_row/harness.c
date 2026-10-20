/* gate: port__hermes_cli_tools_config_py___format_imagegen_model_row — traced copy of format_imagegen_model_row (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * format_imagegen_model_row(const char *model_id,
                                 const char *speed,
                                 const char *strengths,
                                 const char *price,
                                 int width_model,
                                 int width_speed,
                                 int width_strengths);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_imagegen_model_row <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("format_imagegen_model_row");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = format_imagegen_model_row(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("format_imagegen_model_row");
    }
    return 0;
}
