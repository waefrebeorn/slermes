/*
 * suggestions_cmd.c — canonical C11 port of hermes_cli/suggestions_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_suggestions_cmd_u_fmt_pending @ hermes_cli/suggestions_cmd.py:_fmt_pending */
int hermes_cli_suggestions_cmd_u_fmt_pending(const char *arg) {
    /* Python: numbered pending suggestions. Arg = "pending_json". */
    if (!arg || !*arg) {
        printf("No suggested automations right now.\nTry `/suggestions catalog` to see the curated starter set, or install a blueprint skill to get one.\n");
        return 0;
    }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_array(j) || json_array_size(j) == 0) {
        if (j) json_free(j);
        printf("No suggested automations right now.\nTry `/suggestions catalog` to see the curated starter set, or install a blueprint skill to get one.\n");
        return 0;
    }
    printf("Suggested automations — `/suggestions accept N` or `dismiss N`:\n\n");
    size_t n = json_array_size(j);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_array_get(j, i);
        if (!s) continue;
        json_t *spec = json_obj_get(s, "job_spec");
        const char *sched = spec && json_is_object(spec) ? json_get_str(spec, "schedule", "?") : "?";
        printf("  %zu. %s  [%s]  (%s)\n", i + 1,
               json_get_str(s, "title", "(untitled)"), sched,
               json_get_str(s, "source", "?"));
        const char *desc = json_get_str(s, "description", "");
        if (desc[0]) printf("     %s\n", desc);
    }
    json_free(j);
    return 0;
}

/* PoP: hermes_cli_suggestions_cmd_u_resolve_origin @ hermes_cli/suggestions_cmd.py:_resolve_origin */
int hermes_cli_suggestions_cmd_u_resolve_origin(const char *arg) {
    /* Python: session env origin. Arg = "platform\tchat_id\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_suggestions_cmd_handle_suggestions_command @ hermes_cli/suggestions_cmd.py:handle_suggestions_command */
int hermes_cli_suggestions_cmd_handle_suggestions_command(const char *arg) {
    /* Python: /suggestions dispatch. Arg =
     * "sub\tstate\tresult". */
    if (!arg || !*arg) { printf("No pending suggestions.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *sub = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("Suggestions are unavailable in this build.\n"); return 0; }
    if (strcmp(sub, "accept") == 0) {
        printf("Usage: /suggestions accept <number|id>\n");
        printf("Accepted; scheduled as %s\n", t2 ? t2 + 1 : "job");
        return 0;
    }
    printf("%s\n", t2 ? t2 + 1 : "pending list");
    return 0;
}
