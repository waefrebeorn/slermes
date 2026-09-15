/* gate: hermes_cli_kanban_db_py___managed_scratch_path_info — traced copy of kdbport_u_managed_scratch_path_info */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_u_managed_scratch_path_info(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_u_managed_scratch_path_info");
    int result = kdbport_u_managed_scratch_path_info(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_u_managed_scratch_path_info");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_u_managed_scratch_path_info <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
