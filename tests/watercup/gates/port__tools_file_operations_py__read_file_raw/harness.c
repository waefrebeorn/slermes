/* gate: port__tools_file_operations_py__read_file_raw — traced copy of fo_read_file_raw (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int fo_read_file_raw(const char *path, char **out_content, size_t *out_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fo_read_file_raw <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fo_read_file_raw");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = fo_read_file_raw(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fo_read_file_raw");
    }
    return 0;
}
