/* gate: agent_conversation_compression_py___messages_match_scoped_identity — traced copy of _messages_match_scoped_identity */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _messages_match_scoped_identity(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_messages_match_scoped_identity");
    int result = _messages_match_scoped_identity(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_messages_match_scoped_identity");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _messages_match_scoped_identity <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
