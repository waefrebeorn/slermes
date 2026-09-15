/* gate: tools_file_tools_py___is_blocked_device_path — traced copy of file_tools_is_blocked_device_path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool file_tools_is_blocked_device_path(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("file_tools_is_blocked_device_path");
    bool result = file_tools_is_blocked_device_path(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("file_tools_is_blocked_device_path");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s file_tools_is_blocked_device_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
