/* gate: real__hermes_cli_nous_subscription_py___uses_gateway -> real _uses_gateway (int 1/0). JSON section input. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern int nsub_u_uses_gateway(const char *section_json);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("nsub_u_uses_gateway");
        fflush(stdout);
        int r = nsub_u_uses_gateway(argv[i]);
        WCUP_VALINT(r);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("nsub_u_uses_gateway");
    }
    return 0;
}
