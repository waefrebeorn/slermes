/* gate: port__tools_image_generation_tool_py___build_no_backend_setup_message — traced copy of image_gen_build_no_backend_setup_message (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * image_gen_build_no_backend_setup_message(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s image_gen_build_no_backend_setup_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("image_gen_build_no_backend_setup_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = image_gen_build_no_backend_setup_message();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("image_gen_build_no_backend_setup_message");
    }
    return 0;
}
