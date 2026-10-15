/* gate: port__tools_url_safety_py____init__ — traced copy of tools_url_safety_async_init (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int tools_url_safety_async_init(const char *schemes_by_origin_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_url_safety_async_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_url_safety_async_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tools_url_safety_async_init(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tools_url_safety_async_init");
    }
    return 0;
}
