/* gate: cron_notepad_py__clear_notepad — traced copy of cron_notepad_clear_notepad */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_notepad_clear_notepad(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_notepad_clear_notepad");
    int result = cron_notepad_clear_notepad(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_notepad_clear_notepad");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_notepad_clear_notepad <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
