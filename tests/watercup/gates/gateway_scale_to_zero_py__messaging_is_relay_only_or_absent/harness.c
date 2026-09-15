/* gate: gateway_scale_to_zero_py__messaging_is_relay_only_or_absent — traced copy of scale_to_zero_messaging_is_relay_only_or_absent */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int scale_to_zero_messaging_is_relay_only_or_absent(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scale_to_zero_messaging_is_relay_only_or_absent");
    int result = scale_to_zero_messaging_is_relay_only_or_absent(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scale_to_zero_messaging_is_relay_only_or_absent");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scale_to_zero_messaging_is_relay_only_or_absent <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
