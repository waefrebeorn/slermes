/* gate: port__tools_send_message_tool_py___parse_target_ref — traced copy of send_message_parse_target_ref (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int send_message_parse_target_ref(const char *platform_name, const char *target_ref,
                                   char *chat_id_out, size_t chat_id_size,
                                   char *thread_id_out, size_t thread_id_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s send_message_parse_target_ref <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("send_message_parse_target_ref");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = send_message_parse_target_ref(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("send_message_parse_target_ref");
    }
    return 0;
}
