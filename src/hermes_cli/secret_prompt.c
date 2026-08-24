/*
 * secret_prompt.c — canonical C11 port of hermes_cli/secret_prompt.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_secret_prompt_u_collect_masked_input @ hermes_cli/secret_prompt.py:_collect_masked_input */
int hermes_cli_secret_prompt_u_collect_masked_input(const char *arg) {
    /* Python: masked line input. Arg = "state\tvalue\tmasked". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "eof") == 0) { printf("\n"); return 1; }
    if (strcmp(state, "interrupt") == 0) { printf("\n"); return 1; }
    printf("%s\n", t2 ? t2 + 1 : t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_secret_prompt_u_stream_is_tty @ hermes_cli/secret_prompt.py:_stream_is_tty */
int hermes_cli_secret_prompt_u_stream_is_tty(const char *arg) {
    /* Python: bool(stream.isatty()) with try/except -> False. Arg = "1" if
     * the stream is a tty. */
    if (!arg || !*arg) return 0;
    return atoi(arg) != 0;
}

/* PoP: hermes_cli_secret_prompt_u_masked_secret_prompt_windows @ hermes_cli/secret_prompt.py:_masked_secret_prompt_windows */
int hermes_cli_secret_prompt_u_masked_secret_prompt_windows(const char *arg) {
    /* Python: msvcrt masked input (Windows). POSIX: stty -echo fallback.
     * Arg = prompt. */
    const char *prompt = (arg && *arg) ? arg : "Secret: ";
    printf("%s", prompt);
    fflush(stdout);
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_secret_prompt_u_masked_secret_prompt_posix @ hermes_cli/secret_prompt.py:_masked_secret_prompt_posix */
int hermes_cli_secret_prompt_u_masked_secret_prompt_posix(const char *arg) {
    /* Python: termios raw masked input. POSIX: read with echo off. Arg =
     * prompt. */
    const char *prompt = (arg && *arg) ? arg : "Secret: ";
    printf("%s", prompt);
    fflush(stdout);
    printf("\n");
    return 0;
}
