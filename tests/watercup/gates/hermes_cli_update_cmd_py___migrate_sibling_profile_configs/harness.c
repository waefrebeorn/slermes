/* gate: hermes_cli_update_cmd_py___migrate_sibling_profile_configs — traced copy of upd_migrate_sibling_profile_configs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int upd_migrate_sibling_profile_configs(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("upd_migrate_sibling_profile_configs");
    int result = upd_migrate_sibling_profile_configs(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("upd_migrate_sibling_profile_configs");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s upd_migrate_sibling_profile_configs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
