/* gate: port__gateway_profile_routing_py___is_whatsapp_non_user_chat — traced copy of _is_whatsapp_non_user_chat (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _is_whatsapp_non_user_chat(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_whatsapp_non_user_chat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_whatsapp_non_user_chat");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _is_whatsapp_non_user_chat(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_whatsapp_non_user_chat");
    }
    return 0;
}
