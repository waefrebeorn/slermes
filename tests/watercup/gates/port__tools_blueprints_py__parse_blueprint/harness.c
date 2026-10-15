/* gate: port__tools_blueprints_py__parse_blueprint — traced copy of cli_tools_blueprints_parse_blueprint (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_blueprints_parse_blueprint(const char *skill_md_text,
    char *name_out, size_t name_size,
    char *schedule_out, size_t sched_size,
    char *deliver_out, size_t deliver_size,
    char *prompt_out, size_t prompt_size,
    int *no_agent_out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_blueprints_parse_blueprint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_blueprints_parse_blueprint");
    char _s1[256]; _s1[0] = 0;
    char _s3[256]; _s3[0] = 0;
    char _s5[256]; _s5[0] = 0;
    char _s7[256]; _s7[0] = 0;
    int _o9 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_blueprints_parse_blueprint(argv[i], _s1, sizeof(_s1), _s3, sizeof(_s3), _s5, sizeof(_s5), _s7, sizeof(_s7), &_o9);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s1);
    WCUP_VALSTR(_s3);
    WCUP_VALSTR(_s5);
    WCUP_VALSTR(_s7);
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o9); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_blueprints_parse_blueprint");
    }
    return 0;
}
