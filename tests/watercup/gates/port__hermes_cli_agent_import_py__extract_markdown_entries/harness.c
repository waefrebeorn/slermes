/* gate: port__hermes_cli_agent_import_py__extract_markdown_entries — traced copy of ai_extract_markdown_entries (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** ai_extract_markdown_entries(const char *text);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ai_extract_markdown_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ai_extract_markdown_entries");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = ai_extract_markdown_entries(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ai_extract_markdown_entries");
    }
    return 0;
}
