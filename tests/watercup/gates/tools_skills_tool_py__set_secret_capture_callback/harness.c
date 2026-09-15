/* gate: tools_skills_tool_py__set_secret_capture_callback — traced copy of skt_set_secret_capture_callback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int skt_set_secret_capture_callback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("skt_set_secret_capture_callback");
    int result = skt_set_secret_capture_callback(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("skt_set_secret_capture_callback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s skt_set_secret_capture_callback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
