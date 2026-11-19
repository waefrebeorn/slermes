/* gate: port__tools_environments_singularity_py____init__ — traced copy of tools_singularity_init (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int tools_singularity_init(const char *image, const char *cwd, long timeout,
                           double cpu, bool persistent);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_singularity_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_singularity_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = tools_singularity_init(argv[i], argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 3) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10));
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
        WCUP_END("tools_singularity_init");
    }
    return 0;
}
