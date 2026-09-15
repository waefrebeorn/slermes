/* gate: gateway_platforms_qqbot_adapter_py___write_update_response — traced copy of qqbot_u_write_update_response */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int qqbot_u_write_update_response(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("qqbot_u_write_update_response");
    int result = qqbot_u_write_update_response(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("qqbot_u_write_update_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqbot_u_write_update_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
