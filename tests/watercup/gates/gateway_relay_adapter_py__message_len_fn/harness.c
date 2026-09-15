/* gate: gateway_relay_adapter_py__message_len_fn — traced copy of relay_adapter_message_len */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int relay_adapter_message_len(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_adapter_message_len");
    int result = relay_adapter_message_len(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_adapter_message_len");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_adapter_message_len <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
