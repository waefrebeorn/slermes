/* gate: gateway_platforms_qqbot_adapter_py__handle_message — traced copy of qqa_handle_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int qqa_handle_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("qqa_handle_message");
    int result = qqa_handle_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("qqa_handle_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqa_handle_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
