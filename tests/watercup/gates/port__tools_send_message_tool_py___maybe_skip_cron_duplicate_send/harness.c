/* gate: port__tools_send_message_tool_py___maybe_skip_cron_duplicate_send — traced copy of send_message_maybe_skip_cron_duplicate_send (int_other) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int send_message_maybe_skip_cron_duplicate_send(const char *platform, const char *chat_id,
                                                 const char *message, int delivery_interval_minutes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s send_message_maybe_skip_cron_duplicate_send <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("send_message_maybe_skip_cron_duplicate_send");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = send_message_maybe_skip_cron_duplicate_send(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("send_message_maybe_skip_cron_duplicate_send");
    }
    return 0;
}
