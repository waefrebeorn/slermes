/* gate: gateway_platforms_qqbot_chunked_upload_py___part_finish_with_retry — traced copy of gateway_platforms_qqbot_chunke_u_part_finish_with_retry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platforms_qqbot_chunke_u_part_finish_with_retry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platforms_qqbot_chunke_u_part_finish_with_retry");
    int result = gateway_platforms_qqbot_chunke_u_part_finish_with_retry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platforms_qqbot_chunke_u_part_finish_with_retry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platforms_qqbot_chunke_u_part_finish_with_retry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
