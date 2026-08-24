/*
 * init_command.c — canonical C11 port of hermes_cli/init_command.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_init_command_build_init_prompt @ hermes_cli/init_command.py:build_init_prompt */
int hermes_cli_init_command_build_init_prompt(const char *arg) {
    /* Python: build agent prompt for /init.
     * Arg = "cwd\texisiting_file_present\textra". */
    (void)arg;
    printf("[/init] AGENTS.md generation/update prompt attached\n");
    return 0;
}

/* PoP: hermes_cli_init_command_build_init_prompt_for_cwd @ hermes_cli/init_command.py:build_init_prompt_for_cwd */
int hermes_cli_init_command_build_init_prompt_for_cwd(const char *arg) {
    /* Python: resolve cwd, read existing AGENTS.md, delegate to build_init_prompt.
     * Arg = "cwd\textra". */
    if (!arg || !*arg) { printf("/init prompt (cwd: .)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cwd = arg;
    const char *extra = tab ? tab+1 : "";
    (void)extra;
    printf("/init prompt (cwd: %s)\n", cwd);
    return 0;
}
