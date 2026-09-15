/* gate: gateway_platforms_qqbot_chunked_upload_py___upload_one_part — traced copy of gateway_platforms_qqbot_chunke_u_upload_one_part */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platforms_qqbot_chunke_u_upload_one_part(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platforms_qqbot_chunke_u_upload_one_part");
    int result = gateway_platforms_qqbot_chunke_u_upload_one_part(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platforms_qqbot_chunke_u_upload_one_part");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platforms_qqbot_chunke_u_upload_one_part <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
