/* gate: port__gateway_platforms_qqbot_keyboards_py__operator_openid — traced copy of cli_gateway_platforms_qqbot_keyboards_operator_openid (json_param) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"
#include "json.h"
extern void json_free(json_t *);
extern char *json_serialize(const json_t *);
extern json_t *json_parse(const char *, char **);


extern char* cli_gateway_platforms_qqbot_keyboards_operator_openid(json_node_t *event);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_qqbot_keyboards_operator_openid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_qqbot_keyboards_operator_openid");
    json_t *_j0 = json_parse(argv[i], NULL);
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_gateway_platforms_qqbot_keyboards_operator_openid(_j0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }    if (_j0) json_free(_j0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_qqbot_keyboards_operator_openid");
    }
    return 0;
}
