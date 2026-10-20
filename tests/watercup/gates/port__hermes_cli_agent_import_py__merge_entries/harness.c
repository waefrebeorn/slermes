/* gate: port__hermes_cli_agent_import_py__merge_entries — traced copy of ai_merge_entries (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** ai_merge_entries(const char *existing_json, const char *incoming_json,
                        long limit, size_t *out_added,
                        size_t *out_duplicates, size_t *out_overflowed);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ai_merge_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ai_merge_entries");
    size_t _o3 = 0;
    size_t _o4 = 0;
    size_t _o5 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = ai_merge_entries(argv[i], argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), &_o3, &_o4, &_o5);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o3); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o4); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o5); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ai_merge_entries");
    }
    return 0;
}
