/* gate: hermes_cli_setup_whatsapp_cloud_py__run_whatsapp_cloud_setup — traced copy of hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup");
    int result = hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
