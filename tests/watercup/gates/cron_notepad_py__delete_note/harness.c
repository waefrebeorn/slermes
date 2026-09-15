/* gate: cron_notepad_py__delete_note — traced copy of cron_notepad_delete_note */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cron_notepad_delete_note(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cron_notepad_delete_note");
    int result = cron_notepad_delete_note(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cron_notepad_delete_note");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cron_notepad_delete_note <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
