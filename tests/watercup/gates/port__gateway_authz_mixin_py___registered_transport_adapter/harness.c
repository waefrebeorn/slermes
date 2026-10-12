/* gate: port__gateway_authz_mixin_py___registered_transport_adapter — traced copy of gateway_authz_mixin_u_registered_transport_adapter (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int gateway_authz_mixin_u_registered_transport_adapter(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_authz_mixin_u_registered_transport_adapter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("gateway_authz_mixin_u_registered_transport_adapter");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = gateway_authz_mixin_u_registered_transport_adapter(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gateway_authz_mixin_u_registered_transport_adapter");
    }
    return 0;
}
