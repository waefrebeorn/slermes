/* gate: agent_curator_backup_py___restore_cron_skill_links — traced copy of cub_restore_cron_skill_links */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cub_restore_cron_skill_links(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cub_restore_cron_skill_links");
    int result = cub_restore_cron_skill_links(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cub_restore_cron_skill_links");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cub_restore_cron_skill_links <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
