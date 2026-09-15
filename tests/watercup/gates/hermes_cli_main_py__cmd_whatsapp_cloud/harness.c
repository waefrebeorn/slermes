/* gate: hermes_cli_main_py__cmd_whatsapp_cloud — traced copy of main_cmd_whatsapp_cloud */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_cmd_whatsapp_cloud(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_cmd_whatsapp_cloud");
    int result = main_cmd_whatsapp_cloud(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_cmd_whatsapp_cloud");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_cmd_whatsapp_cloud <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
