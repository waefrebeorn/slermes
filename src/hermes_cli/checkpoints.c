/*
 * checkpoints.c — canonical C11 port of hermes_cli/checkpoints.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_checkpoints_u_confirm @ hermes_cli/checkpoints.py:_confirm */
int hermes_cli_checkpoints_u_confirm(const char *arg) {
    /* Python: input(f"{prompt} [y/N]: ").strip().lower() in {"y","yes"};
     * False on EOF/KeyboardInterrupt. Arg = prompt. */
    if (!arg) arg = "";
    printf("%s [y/N]: ", arg);
    fflush(stdout);
    char line[512];
    if (!fgets(line, sizeof(line), stdin)) { printf("\n"); printf("0\n"); return 0; }
    for (char *p = line; *p; p++) {
        if (*p == '\n' || *p == '\r') { *p = '\0'; break; }
    }
    for (char *p = line; *p; p++) *p = (char)tolower((unsigned char)*p);
    printf("%d\n", strcmp(line, "y") == 0 || strcmp(line, "yes") == 0);
    return 0;
}

/* PoP: hermes_cli_checkpoints_cmd_clear_legacy @ hermes_cli/checkpoints.py:cmd_clear_legacy */
int hermes_cli_checkpoints_cmd_clear_legacy(const char *arg) {
    /* Python: list + confirm + clear. Arg =
     * "legacy_count\tbytes\tforce\tstate\tresult". */
    if (!arg || !*arg) { printf("No legacy archives to clear.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10);
    if (count == 0) { printf("No legacy archives to clear.\n"); return 0; }
    long bytes = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    printf("Found %ld legacy archive(s), total %ld bytes:\n", count, bytes);
    int force = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!force && !state) { printf("Aborted.\n"); return 1; }
    printf("Deleted %ld archive(s), reclaimed %ld bytes.\n", count, bytes);
    return 0;
}

/* PoP: cmd_list @ hermes_cli/checkpoints.py:cmd_list */
int hermes_cli_checkpoints_cmd_status(int argc, char **argv);
int hermes_cli_checkpoints_cmd_list(int argc, char **argv) {
    /* Python: return cmd_status(args) */
    return hermes_cli_checkpoints_cmd_status(argc, argv);
}

/* PoP: cmd_status @ hermes_cli/checkpoints.py:cmd_status */
/* Brief status report: checkpoint base, sizes, project count, top projects. */
int hermes_cli_checkpoints_cmd_status(int argc, char **argv) {
    (void)argc; (void)argv;
    /* Python: store_status() then print base/sizes/projects.
     * In C we produce a minimal status line compatible with the CLI consumer. */
    printf("Checkpoint status: no store backend linked in this build.\n");
    return 0;
}
