/* gate: hermes_cli_auth_py__clear_codex_pool_quota_cooldowns — traced copy of auth_clear_codex_pool_quota_cooldowns */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int auth_clear_codex_pool_quota_cooldowns(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("auth_clear_codex_pool_quota_cooldowns");
    int result = auth_clear_codex_pool_quota_cooldowns(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("auth_clear_codex_pool_quota_cooldowns");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_clear_codex_pool_quota_cooldowns <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
