/* gate: port__hermes_cli_web_server_py___ensure_managed_root — traced copy of ws_ensure_managed_root (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int ws_ensure_managed_root(const char *path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_ensure_managed_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ws_ensure_managed_root");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = ws_ensure_managed_root(argv[i]);
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
        WCUP_END("ws_ensure_managed_root");
    }
    return 0;
}
