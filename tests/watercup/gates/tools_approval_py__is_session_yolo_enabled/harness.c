/* gate: tools_approval_py__is_session_yolo_enabled — traced copy of apr2_is_session_yolo_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool apr2_is_session_yolo_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr2_is_session_yolo_enabled");
    bool result = apr2_is_session_yolo_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr2_is_session_yolo_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_is_session_yolo_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
