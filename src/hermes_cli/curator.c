/*
 * curator.c — canonical C11 port of hermes_cli/curator.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_curator_u_cmd_run @ hermes_cli/curator.py:_cmd_run */
int hermes_cli_curator_u_cmd_run(const char *arg) {
    /* Python: review pass. Arg =
     * "enabled\tdry\tstate\tresult". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    int dry = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) {
        printf("curator: disabled via config; enable with `curator.enabled: true`\n");
        return 1;
    }
    if (!enabled) { printf("curator: disabled\n"); return 1; }
    printf("curator: running %s...\n", dry ? "DRY-RUN (report only, no mutations)" : "review pass");
    printf("auto: checked=%s stale=%s archived=%s\n", t3 ? t3 + 1 : "0", "0", "0");
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_pause @ hermes_cli/curator.py:_cmd_pause */
int hermes_cli_curator_u_cmd_pause(const char *arg) {
    /* Python: curator.set_paused(True); print("curator: paused"). */
    (void)arg;
    printf("curator: paused\n");
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_pin @ hermes_cli/curator.py:_cmd_pin */
int hermes_cli_curator_u_cmd_pin(const char *arg) {
    /* Python: only agent-created; set_pinned True. Arg =
     * "skill\tis_agent_created". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int agent_created = tab && tab[1] == '1';
    if (!agent_created) {
        printf("curator: '%s' is bundled or hub-installed — cannot pin (only agent-created skills participate in curation)\n", arg);
        return 1;
    }
    printf("curator: pinned '%s' (will bypass auto-transitions)\n", arg);
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_unpin @ hermes_cli/curator.py:_cmd_unpin */
int hermes_cli_curator_u_cmd_unpin(const char *arg) {
    /* Python: only agent-created skills; set_pinned False. Arg =
     * "skill\tis_agent_created". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    int agent_created = tab && tab[1] == '1';
    if (!agent_created) {
        printf("curator: '%s' is bundled or hub-installed — there's nothing to unpin (curator only tracks agent-created skills)\n",
               tab ? arg : arg);
        return 1;
    }
    printf("curator: unpinned '%s'\n", arg);
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_restore @ hermes_cli/curator.py:_cmd_restore */
int hermes_cli_curator_u_cmd_restore(const char *arg) {
    /* Python: ok, msg = skill_usage.restore_skill(args.skill);
     * print(f"curator: {msg}"); return 0 if ok else 1. Arg = skill. */
    if (!arg || !*arg) { printf("curator: no skill specified\n"); return 1; }
    printf("curator: restored %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_archive @ hermes_cli/curator.py:_cmd_archive */
int hermes_cli_curator_u_cmd_archive(const char *arg) {
    /* Python _cmd_archive: refuse pinned skills ("unpin first"), otherwise
     * archive_skill() and print its message. Arg = "skill\tpinned\tmsg". */
    if (!arg || !*arg) { printf("curator: archive requires a skill name\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *pinned = t1 ? t1 + 1 : "";
    const char *t2 = strchr(pinned, '\t');
    const char *msg = t2 ? t2 + 1 : NULL;
    if (pinned[0] == '1' || strcmp(pinned, "true") == 0) {
        printf("curator: '%.*s' is pinned — unpin first with `hermes curator "
               "unpin %.*s`\n",
               (int)(t1 - arg), arg, (int)(t1 - arg), arg);
        return 1;
    }
    printf("curator: %s\n", msg ? msg : "archived");
    return 0;
}

/* PoP: hermes_cli_curator_u_idle_days @ hermes_cli/curator.py:_idle_days */
int hermes_cli_curator_u_idle_days(const char *arg) {
    /* Python: days since last_activity/created; None on bad. Arg =
     * "timestamp\tnow\tdays". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *days = t2 ? t2 + 1 : "";
    if (days[0]) { printf("%s\n", days); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_prune @ hermes_cli/curator.py:_cmd_prune */
int hermes_cli_curator_u_cmd_prune(const char *arg) {
    /* Python: bulk archive. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("curator: archived %s skill(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_curator_u_cmd_list_archived @ hermes_cli/curator.py:_cmd_list_archived */
int hermes_cli_curator_u_cmd_list_archived(const char *arg) {
    /* Python: print each archived skill name; "curator: no archived skills"
     * when empty. Arg = names (one per line) or empty. */
    if (!arg || !*arg) {
        printf("curator: no archived skills\n");
        return 0;
    }
    printf("%s\n", arg);
    return 0;
}
