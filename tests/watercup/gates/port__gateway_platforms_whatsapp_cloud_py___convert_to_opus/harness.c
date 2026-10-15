/* gate: port__gateway_platforms_whatsapp_cloud_py___convert_to_opus — traced copy of wac_convert_to_opus (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * wac_convert_to_opus(const char *mp3_path, const char *out_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wac_convert_to_opus <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("wac_convert_to_opus");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = wac_convert_to_opus(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wac_convert_to_opus");
    }
    return 0;
}
