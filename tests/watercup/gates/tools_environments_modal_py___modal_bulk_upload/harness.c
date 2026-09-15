/* gate: tools_environments_modal_py___modal_bulk_upload — traced copy of tools_environments_modal_u_modal_bulk_upload */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_environments_modal_u_modal_bulk_upload(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_environments_modal_u_modal_bulk_upload");
    int result = tools_environments_modal_u_modal_bulk_upload(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_environments_modal_u_modal_bulk_upload");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_environments_modal_u_modal_bulk_upload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
