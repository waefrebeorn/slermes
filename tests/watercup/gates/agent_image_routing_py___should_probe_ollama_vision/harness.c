/* gate: agent_image_routing_py___should_probe_ollama_vision — traced copy of _should_probe_ollama_vision */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _should_probe_ollama_vision(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_should_probe_ollama_vision");
    int result = _should_probe_ollama_vision(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_should_probe_ollama_vision");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _should_probe_ollama_vision <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
