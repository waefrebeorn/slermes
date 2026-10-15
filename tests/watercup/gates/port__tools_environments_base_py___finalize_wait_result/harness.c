/* gate: port__tools_environments_base_py___finalize_wait_result — traced copy of envb_finalize_wait_result (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * envb_finalize_wait_result(const char *rendered, long returncode,
                                 long total_chars, const char *spill_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s envb_finalize_wait_result <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("envb_finalize_wait_result");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = envb_finalize_wait_result((int)strtol(argv[i], NULL, 10), (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("envb_finalize_wait_result");
    }
    return 0;
}
