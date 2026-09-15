/* gate: tools_bot_mode_probe_py__stored_bot_chat_prompt_needs_upgrade — traced copy of stored_bot_chat_prompt_needs_upgrade */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int stored_bot_chat_prompt_needs_upgrade(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("stored_bot_chat_prompt_needs_upgrade");
    int result = stored_bot_chat_prompt_needs_upgrade(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stored_bot_chat_prompt_needs_upgrade");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stored_bot_chat_prompt_needs_upgrade <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
