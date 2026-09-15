/* gate: tools_skills_sync_py___is_tracked_user_modification — traced copy of sys_is_tracked_user_modification */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sys_is_tracked_user_modification(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sys_is_tracked_user_modification");
    int result = sys_is_tracked_user_modification(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sys_is_tracked_user_modification");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sys_is_tracked_user_modification <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
