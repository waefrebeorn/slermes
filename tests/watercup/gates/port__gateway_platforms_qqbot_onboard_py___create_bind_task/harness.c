/* gate: port__gateway_platforms_qqbot_onboard_py___create_bind_task — traced copy of qqbot_onboard__create_bind_task (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int qqbot_onboard__create_bind_task(char *task_id, size_t task_id_len,
                                      char *aes_key, size_t key_len);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s qqbot_onboard__create_bind_task <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("qqbot_onboard__create_bind_task");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = qqbot_onboard__create_bind_task(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("qqbot_onboard__create_bind_task");
    }
    return 0;
}
