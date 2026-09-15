/* gate: tools_checkpoint_manager_py___delete_ref — traced copy of _delete_ref */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _delete_ref(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_delete_ref");
    int result = _delete_ref(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_delete_ref");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _delete_ref <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
