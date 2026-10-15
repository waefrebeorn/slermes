/* gate: port__gateway_platforms_qqbot_keyboards_py__parse_update_prompt_button_data — traced copy of cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char* cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data(const char *button_data);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_gateway_platforms_qqbot_keyboards_parse_update_prompt_button_data");
    }
    return 0;
}
