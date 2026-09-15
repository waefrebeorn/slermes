/* gate: hermes_cli_webhook_py___cmd_test — traced copy of hcli_webhook_cmd_test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hcli_webhook_cmd_test(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hcli_webhook_cmd_test");
    int result = hcli_webhook_cmd_test(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hcli_webhook_cmd_test");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_webhook_cmd_test <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
