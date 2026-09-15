/* gate: hermes_cli_nous_subscription_py__video_gen — traced copy of nsub_video_gen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nsub_video_gen(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nsub_video_gen");
    int result = nsub_video_gen(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nsub_video_gen");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nsub_video_gen <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
