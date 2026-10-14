/* gate: port__agent_context_compressor_py___derive_auto_focus_topic — traced copy of cli_agent_context_compressor__derive_auto_focus_topic (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_agent_context_compressor__derive_auto_focus_topic(const char **recent_messages, int num_messages, char *buf, size_t bufsize);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_context_compressor__derive_auto_focus_topic <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_context_compressor__derive_auto_focus_topic");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_agent_context_compressor__derive_auto_focus_topic(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_context_compressor__derive_auto_focus_topic");
    }
    return 0;
}
