/* gate: port__tools_tts_tool_py___terminate_command_tts_process_tree — traced copy of tts_terminate_command_tts_process_tree (int_int) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int tts_terminate_command_tts_process_tree(int proc_pid);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_terminate_command_tts_process_tree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_terminate_command_tts_process_tree");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tts_terminate_command_tts_process_tree((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_terminate_command_tts_process_tree");
    }
    return 0;
}
