/* gate: port__agent_pet_render_py____init__ — traced copy of petr_init (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int petr_init(const char *spritesheet, const char *mode, double scale,
              long unicode_cols, long frame_w, long frame_h, long frames_per_state);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s petr_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("petr_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = petr_init(argv[i], argv[2 + (i - 2 + 1) % 5], strtod(argv[2 + (i - 2 + 2) % 5], NULL), (long)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10), (long)strtol(argv[2 + (i - 2 + 6) % 5], NULL, 10));
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
        WCUP_END("petr_init");
    }
    return 0;
}
