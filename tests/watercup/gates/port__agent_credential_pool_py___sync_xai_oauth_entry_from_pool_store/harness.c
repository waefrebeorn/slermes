/* gate: port__agent_credential_pool_py___sync_xai_oauth_entry_from_pool_store — traced copy of agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store");
    }
    return 0;
}
