/* gate: hermes_cli_gateway_py___launchctl_label_supervising_process — traced copy of _launchctl_label_supervising_process */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _launchctl_label_supervising_process(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_launchctl_label_supervising_process");
    int result = _launchctl_label_supervising_process(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_launchctl_label_supervising_process");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _launchctl_label_supervising_process <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
