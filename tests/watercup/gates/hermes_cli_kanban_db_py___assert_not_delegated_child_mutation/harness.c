/* gate: hermes_cli_kanban_db_py___assert_not_delegated_child_mutation — traced copy of kdbport_u_assert_not_delegated_child_mutation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kdbport_u_assert_not_delegated_child_mutation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("kdbport_u_assert_not_delegated_child_mutation");
    int result = kdbport_u_assert_not_delegated_child_mutation(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kdbport_u_assert_not_delegated_child_mutation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdbport_u_assert_not_delegated_child_mutation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
