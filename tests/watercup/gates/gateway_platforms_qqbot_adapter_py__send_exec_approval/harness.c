/* gate: gateway_platforms_qqbot_adapter_py__send_exec_approval — traced copy of qqbot_send_exec_approval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int qqbot_send_exec_approval(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("qqbot_send_exec_approval");
    int result = qqbot_send_exec_approval(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("qqbot_send_exec_approval");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqbot_send_exec_approval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
