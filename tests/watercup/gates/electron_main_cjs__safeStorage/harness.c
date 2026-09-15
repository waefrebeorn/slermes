/* gate: electron_main_cjs__safeStorage — traced copy of desktop_auth_ticket_set */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool desktop_auth_ticket_set(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("desktop_auth_ticket_set");
    bool result = desktop_auth_ticket_set(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("desktop_auth_ticket_set");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s desktop_auth_ticket_set <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
