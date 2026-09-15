/* gate: gateway_relay_adapter_py__stream_is_message_for_chat — traced copy of relay_adapter_stream_is_message_for_chat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool relay_adapter_stream_is_message_for_chat(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_adapter_stream_is_message_for_chat");
    bool result = relay_adapter_stream_is_message_for_chat(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_adapter_stream_is_message_for_chat");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_adapter_stream_is_message_for_chat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
