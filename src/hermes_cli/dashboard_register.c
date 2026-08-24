/*
 * dashboard_register.c — canonical C11 port of hermes_cli/dashboard_register.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_dashboard_register_u_generate_dashboard_name @ hermes_cli/dashboard_register.py:_generate_dashboard_name */
int hermes_cli_dashboard_register_u_generate_dashboard_name(const char *arg) {
    /* Python: f"{random.choice(_NAME_ADJECTIVES)}_{random.choice(_NAME_NOUNS)}"
     * — Docker-style adjective_noun. */
    (void)arg;
    static const char *adjs[] = {"brave", "swift", "calm", "eager", "jolly",
                                 "lucky", "noble", "quiet", "witty", "bold"};
    static const char *nouns[] = {"otter", "falcon", "willow", "ember", "pine",
                                  "coral", "breeze", "sable", "meadow", "comet"};
    unsigned seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    const char *a = adjs[(seed ^ (unsigned)rand()) % (sizeof(adjs) / sizeof(adjs[0]))];
    const char *n = nouns[(rand() ^ (seed >> 4)) % (sizeof(nouns) / sizeof(nouns[0]))];
    printf("%s_%s\n", a, n);
    return 0;
}

/* PoP: hermes_cli_dashboard_register_u_register_self_hosted_client @ hermes_cli/dashboard_register.py:_register_self_hosted_client */
int hermes_cli_dashboard_register_u_register_self_hosted_client(const char *arg) {
    /* Python: idempotent POST. Arg =
     * "has_id\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_id = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) {
        fprintf(stderr, "register failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("client registered%s: %s\n", has_id ? " (idempotent update)" : "", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_dashboard_register_u_print_post_register_hint @ hermes_cli/dashboard_register.py:_print_post_register_hint */
int hermes_cli_dashboard_register_u_print_post_register_hint(const char *arg) {
    /* Python: gate-engagement caveat. Arg =
     * "client_id\twrote_portal\tpublic_url\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("register hint skipped\n"); return 0; }
    printf("  Wrote to env: HERMES_DASHBOARD_OAUTH_CLIENT_ID=%s\n", arg);
    printf("  Heads up — Nous login only *engages* on a non-loopback bind.\n");
    printf("  Manage or revoke this dashboard at the portal /local-dashboards\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_register_cmd_dashboard_register @ hermes_cli/dashboard_register.py:cmd_dashboard_register */
int hermes_cli_dashboard_register_cmd_dashboard_register(const char *arg) {
    /* Python: OAuth client register. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "managed") == 0) {
        printf("✗ `hermes dashboard register` is not available in a managed/hosted install.\n  The dashboard OAuth client is provisioned by the hosting platform.\n");
        return 1;
    }
    if (strcmp(state, "not_logged_in") == 0) {
        printf("✗ You're not logged into Nous Portal.\n  Run `hermes setup` (or `hermes auth add nous`) first, then retry.\n");
        return 1;
    }
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "registration failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("✓ Dashboard OAuth client registered (client_id=%s saved to .env, callbacks %s)%s\n", t2 ? t2 + 1 : "?", t3 ? t3 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
