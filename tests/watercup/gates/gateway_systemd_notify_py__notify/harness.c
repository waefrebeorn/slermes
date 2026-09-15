/* gate: gateway_systemd_notify_py__notify — traced copy of sd_n_notify */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sd_n_notify(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sd_n_notify");
    int result = sd_n_notify(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sd_n_notify");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sd_n_notify <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
