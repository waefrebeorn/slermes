/* gate: port__tools_computer_use_tool_py___image_dimensions_from_b64 — traced copy of tools_computer_use_tool_u_image_dimensions_from_b64 (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int tools_computer_use_tool_u_image_dimensions_from_b64(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_computer_use_tool_u_image_dimensions_from_b64 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_computer_use_tool_u_image_dimensions_from_b64");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_cap = tmpfile();
    if (_cap) dup2(fileno(_cap), 1);
    int result = tools_computer_use_tool_u_image_dimensions_from_b64(argv[i]);
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
        WCUP_END("tools_computer_use_tool_u_image_dimensions_from_b64");
    }
    return 0;
}
