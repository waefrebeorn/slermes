/* gate: gateway_relay_adapter_py__native_task_cards_enabled — traced copy of relay_adapter_native_task_cards_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool relay_adapter_native_task_cards_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_adapter_native_task_cards_enabled");
    bool result = relay_adapter_native_task_cards_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_adapter_native_task_cards_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_adapter_native_task_cards_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
