/* gate: gateway_stream_consumer_py__already_sent — traced copy of sc_already_sent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sc_already_sent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sc_already_sent");
    int result = sc_already_sent(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sc_already_sent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sc_already_sent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
