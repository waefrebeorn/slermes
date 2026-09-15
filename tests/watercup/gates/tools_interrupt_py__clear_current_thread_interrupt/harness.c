/* gate: tools_interrupt_py__clear_current_thread_interrupt — traced copy of tools_interrupt_clear_current_thread_interrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_interrupt_clear_current_thread_interrupt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_interrupt_clear_current_thread_interrupt");
    int result = tools_interrupt_clear_current_thread_interrupt(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_interrupt_clear_current_thread_interrupt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_interrupt_clear_current_thread_interrupt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
