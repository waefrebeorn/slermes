/* gate: port__hermes_cli_observability_shared_metrics_send_config_py__reset_warning_latch_for_tests — traced copy of reset_warning_latch_for_tests (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * reset_warning_latch_for_tests(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reset_warning_latch_for_tests <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("reset_warning_latch_for_tests");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = reset_warning_latch_for_tests();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reset_warning_latch_for_tests");
    }
    return 0;
}
