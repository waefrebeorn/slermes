/* gate: port__gateway_platforms_yuanbao_proto_py___dbg — traced copy of yuanbao_proto_dbg (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void yuanbao_proto_dbg(const char *label, const unsigned char *data, size_t len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yuanbao_proto_dbg <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("yuanbao_proto_dbg");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    yuanbao_proto_dbg(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("yuanbao_proto_dbg");
    }
    return 0;
}
