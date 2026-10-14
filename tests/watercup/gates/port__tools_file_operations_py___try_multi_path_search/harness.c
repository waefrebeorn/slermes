/* gate: port__tools_file_operations_py___try_multi_path_search — traced copy of fo_try_multi_path_search (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int fo_try_multi_path_search(const char **paths, int count, char *out_found, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_try_multi_path_search <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_try_multi_path_search");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = fo_try_multi_path_search(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_try_multi_path_search");
    }
    return 0;
}
