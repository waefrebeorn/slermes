/* gate: gateway_stream_consumer_py___stream_is_message — traced copy of _stream_is_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _stream_is_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_stream_is_message");
    int result = _stream_is_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_stream_is_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _stream_is_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
