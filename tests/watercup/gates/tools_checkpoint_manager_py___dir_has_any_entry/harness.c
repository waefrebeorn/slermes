/* gate: tools_checkpoint_manager_py___dir_has_any_entry — traced copy of tools_checkpoint_manager_u_dir_has_any_entry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_checkpoint_manager_u_dir_has_any_entry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_checkpoint_manager_u_dir_has_any_entry");
    int result = tools_checkpoint_manager_u_dir_has_any_entry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_checkpoint_manager_u_dir_has_any_entry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_checkpoint_manager_u_dir_has_any_entry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
