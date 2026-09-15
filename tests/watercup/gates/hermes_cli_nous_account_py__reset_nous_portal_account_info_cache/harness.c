/* gate: hermes_cli_nous_account_py__reset_nous_portal_account_info_cache — traced copy of nous_reset_nous_portal_account_info_cache */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_reset_nous_portal_account_info_cache(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_reset_nous_portal_account_info_cache");
    int result = nous_reset_nous_portal_account_info_cache(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_reset_nous_portal_account_info_cache");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_reset_nous_portal_account_info_cache <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
