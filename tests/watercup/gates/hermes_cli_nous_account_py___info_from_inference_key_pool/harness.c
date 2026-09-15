/* gate: hermes_cli_nous_account_py___info_from_inference_key_pool — traced copy of nous_u_info_from_inference_key_pool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nous_u_info_from_inference_key_pool(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nous_u_info_from_inference_key_pool");
    int result = nous_u_info_from_inference_key_pool(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nous_u_info_from_inference_key_pool");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nous_u_info_from_inference_key_pool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
