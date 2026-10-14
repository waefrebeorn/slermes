/* gate: port__tools_osv_check_py___parse_package_from_args — traced copy of cli_tools_osv_check__parse_package_from_args (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_tools_osv_check__parse_package_from_args(const char **args, int arg_count, const char *ecosystem,
    char *package_out, size_t package_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_osv_check__parse_package_from_args <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_osv_check__parse_package_from_args");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_osv_check__parse_package_from_args(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_osv_check__parse_package_from_args");
    }
    return 0;
}
