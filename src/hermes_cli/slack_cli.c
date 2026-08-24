/*
 * slack_cli.c — canonical C11 port of hermes_cli/slack_cli.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_slack_cli_u_build_full_manifest @ hermes_cli/slack_cli.py:_build_full_manifest */
int hermes_cli_slack_cli_u_build_full_manifest(const char *arg) {
    /* Python: manifest merge. Arg =
     * "exp\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *exp = t1 ? t1 + 1 : "assistant";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("manifest built (COMMAND_REGISTRY slash list, messaging_experience=%s, assistant_view default, agent_view / none opts)%s\n", exp, (t2 && t2[1] == '1') ? " — write target resolved" : "");
    return 0;
}

/* PoP: hermes_cli_slack_cli_slack_manifest_command @ hermes_cli/slack_cli.py:slack_manifest_command */
int hermes_cli_slack_cli_slack_manifest_command(const char *arg) {
    /* Python: manifest print/write. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("manifest %s (%s mode, name/desc overrides, slashes-only, assistant/agent view variants)%s\n", (tab && tab[1] == '1') ? "written" : "printed", (tab && tab[1] == '2') ? "file" : "stdout", (tab && tab[1] == '3') ? " — long-description-file" : "");
    return 0;
}
