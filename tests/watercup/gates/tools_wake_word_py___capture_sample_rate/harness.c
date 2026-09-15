/* gate: tools_wake_word_py___capture_sample_rate — traced copy of wake_word_capture_sample_rate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wake_word_capture_sample_rate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("wake_word_capture_sample_rate");
    int result = wake_word_capture_sample_rate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wake_word_capture_sample_rate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wake_word_capture_sample_rate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
