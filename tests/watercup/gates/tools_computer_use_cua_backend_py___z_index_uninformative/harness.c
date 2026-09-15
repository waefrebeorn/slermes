/* gate: tools_computer_use_cua_backend_py___z_index_uninformative — traced copy of cua_u_z_index_uninformative */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_u_z_index_uninformative(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_u_z_index_uninformative");
    int result = cua_u_z_index_uninformative(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_u_z_index_uninformative");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_u_z_index_uninformative <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
