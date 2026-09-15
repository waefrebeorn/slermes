/* gate: agent_transports___init___py__register_transport — traced copy of tri_register_transport */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tri_register_transport(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("tri_register_transport");
    int result = tri_register_transport(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tri_register_transport");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tri_register_transport <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
