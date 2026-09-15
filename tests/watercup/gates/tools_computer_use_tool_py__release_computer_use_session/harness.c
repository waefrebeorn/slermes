/* gate: tools_computer_use_tool_py__release_computer_use_session — traced copy of cut_release_computer_use_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cut_release_computer_use_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cut_release_computer_use_session");
    int result = cut_release_computer_use_session(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cut_release_computer_use_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cut_release_computer_use_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
