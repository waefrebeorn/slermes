/* gate: gateway_platforms_whatsapp_cloud_py__send_typing — traced copy of wac_send_typing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wac_send_typing(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wac_send_typing");
    int result = wac_send_typing(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wac_send_typing");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wac_send_typing <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
