/* gate: port__hermes_cli_bundles_py___cmd_list — traced copy of hcli_bundles_cmd_list (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int hcli_bundles_cmd_list(const char *bundles_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_bundles_cmd_list <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hcli_bundles_cmd_list");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = hcli_bundles_cmd_list(argv[i]);
    fflush(stdout);
    if (_cap) {
        dup2(_sv, 1); close(_sv);
        rewind(_cap);
        char _ob[4096]; size_t _on = 0; int _ch;
        while ((_ch = fgetc(_cap)) != EOF && _on < sizeof(_ob) - 1)
            _ob[_on++] = (char)_ch;
        _ob[_on] = '\0';
        /* strip the trailing newline the port emits */
        while (_on && (_ob[_on-1] == '\n' || _ob[_on-1] == '\r'))
            _ob[--_on] = '\0';
        WCUP_VALSTR(_ob);
        fclose(_cap);
    }


        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hcli_bundles_cmd_list");
    }
    return 0;
}
