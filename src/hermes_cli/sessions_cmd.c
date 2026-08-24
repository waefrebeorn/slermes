/*
 * sessions_cmd.c — canonical C11 port of hermes_cli/sessions_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_sessions_cmd_u_m @ hermes_cli/sessions_cmd.py:_m */
int hermes_cli_sessions_cmd_u_m(const char *arg) {
    /* Python: lazy hermes_cli.main reference. The C core has no module
     * object — returns 1 to signal the module reference resolves. */
    (void)arg;
    printf("main\n");
    return 0;
}

/* PoP: hermes_cli_sessions_cmd_u_confirm_prompt @ hermes_cli/sessions_cmd.py:_confirm_prompt */
int hermes_cli_sessions_cmd_u_confirm_prompt(const char *arg) {
    /* Python: prompt for y/N confirmation, safe against non-TTY env.
     * Arg = "prompt_text". Reads stdin; y/yes -> 1, else 0. */
    if (!arg || !*arg) arg = "Proceed? [y/N] ";
    fprintf(stdout, "%s", arg);
    fflush(stdout);
    if (!isatty(STDIN_FILENO)) { printf("0\n"); return 0; }
    char buf[16] = {0};
    if (!fgets(buf, sizeof(buf), stdin)) { printf("0\n"); return 0; }
    for (char *p = buf; *p; p++) *p = tolower((unsigned char)*p);
    int yes = strstr(buf, "y") != NULL;
    printf("%d\n", yes);
    return yes;
}
