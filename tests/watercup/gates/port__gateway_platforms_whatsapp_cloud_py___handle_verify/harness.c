/* gate: port__gateway_platforms_whatsapp_cloud_py___handle_verify — traced copy of cli_gateway_platforms_wecom_callback__handle_verify (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_gateway_platforms_wecom_callback__handle_verify(const char *msg_signature, const char *timestamp,
                                                         const char *nonce, const char *echostr,
                                                         json_node_t *apps, char *buf, size_t bufsz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_wecom_callback__handle_verify <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_wecom_callback__handle_verify");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_gateway_platforms_wecom_callback__handle_verify(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_wecom_callback__handle_verify");
    }
    return 0;
}
