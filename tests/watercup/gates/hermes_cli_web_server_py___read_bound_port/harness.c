/* gate: hermes_cli_web_server_py___read_bound_port — traced copy of web_read_bound_port */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int web_read_bound_port(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("web_read_bound_port");
    int result = web_read_bound_port(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("web_read_bound_port");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_read_bound_port <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
