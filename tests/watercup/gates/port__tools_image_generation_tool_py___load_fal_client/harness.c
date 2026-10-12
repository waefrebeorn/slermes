/* gate: port__tools_image_generation_tool_py___load_fal_client — traced copy of image_gen_load_fal_client (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int image_gen_load_fal_client(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s image_gen_load_fal_client <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("image_gen_load_fal_client");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = image_gen_load_fal_client();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("image_gen_load_fal_client");
    }
    return 0;
}
