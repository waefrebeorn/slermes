/* gate: gateway_relay_adapter_py__supports_native_task_cards — traced copy of relay_adapter_supports_native_task_cards */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool relay_adapter_supports_native_task_cards(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_adapter_supports_native_task_cards");
    bool result = relay_adapter_supports_native_task_cards(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_adapter_supports_native_task_cards");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_adapter_supports_native_task_cards <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
