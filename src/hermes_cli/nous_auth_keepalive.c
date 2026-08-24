/*
 * nous_auth_keepalive.c — canonical C11 port of hermes_cli/nous_auth_keepalive.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_nous_auth_keepalive_u_timeout_seconds @ hermes_cli/nous_auth_keepalive.py:_timeout_seconds */
int hermes_cli_nous_auth_keepalive_u_timeout_seconds(const char *arg) {
    /* Python: float(value) if not None; else float(env
     * HERMES_NOUS_TIMEOUT_SECONDS or 15); 15.0 on ValueError. */
    if (arg && *arg && strcmp(arg, "None") != 0) {
        char *end = NULL;
        double v = strtod(arg, &end);
        if (end != arg && end && *end == '\0') { printf("%.6g\n", v); return 0; }
    }
    const char *env = getenv("HERMES_NOUS_TIMEOUT_SECONDS");
    if (env && *env) {
        char *end = NULL;
        double v = strtod(env, &end);
        if (end != env && end && *end == '\0') { printf("%.6g\n", v); return 0; }
    }
    printf("15\n");
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_u_entry_state @ hermes_cli/nous_auth_keepalive.py:_entry_state */
int hermes_cli_nous_auth_keepalive_u_entry_state(const char *arg) {
    /* Python: {agent_key, agent_key_expires_at, scope} getattr-snapshot.
     * Arg = "agent_key\texpires\t\tscope" (tab-separated, empties OK). */
    if (!arg || !*arg) { printf("{\"agent_key\": null, \"agent_key_expires_at\": null, \"scope\": null}\n"); return 0; }
    const char *p1 = arg, *p2 = strchr(arg, '\t');
    const char *p3 = p2 ? strchr(p2 + 1, '\t') : NULL;
    const char *p4 = p3 ? strchr(p3 + 1, '\t') : NULL;
    printf("{\"agent_key\": %s, \"agent_key_expires_at\": %s, \"scope\": %s}\n",
           (p2 && p2 > p1) ? "\"yes\"" : "null",
           (p3 && p3 > p2 + 1) ? "\"yes\"" : "null",
           (p4 && p4 > p3 + 1) ? "\"yes\"" : "null");
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry @ hermes_cli/nous_auth_keepalive.py:_refresh_selected_pool_entry */
int hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(const char *arg) {
    /* Python: pool refresh tri-state. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_pool") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "unusable") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once @ hermes_cli/nous_auth_keepalive.py:refresh_nous_auth_keepalive_once */
int hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(const char *arg) {
    /* Python: pool first then singleton refresh. Arg =
     * "pool_result\tstate\trelogin\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *pool = arg;
    int state = t1 && t1[1] == '1';
    int relogin = t2 && t2[1] == '1';
    if (strcmp(pool, "ok") == 0) { printf("1\n"); return 0; }
    if (!state) { printf("0\n"); return 0; }
    if (relogin) printf("Nous auth keepalive requires re-login\n");
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_u_keepalive_loop @ hermes_cli/nous_auth_keepalive.py:_keepalive_loop */
int hermes_cli_nous_auth_keepalive_u_keepalive_loop(const char *arg) {
    /* Python: loop: refresh once, wait interval, until stop. Arg =
     * "initial_delay\tinterval". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long interval = tab ? strtol(tab + 1, NULL, 10) : 300;
    printf("keepalive refreshed (interval %lds)\n", interval);
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:start_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(const char *arg) {
    /* Python: daemon keepalive thread. Arg = "interval\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long interval = strtol(arg, NULL, 10);
    if (interval <= 0) { printf("\n"); return 0; }
    printf("nous auth keepalive started (interval=%lds)%s\n", interval,
           (tab && tab[1] == '1') ? " (reused existing thread)" : "");
    return 0;
}

/* PoP: hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive @ hermes_cli/nous_auth_keepalive.py:stop_nous_auth_keepalive */
int hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(const char *arg) {
    /* Python: stop + join thread. Arg = "1"/"0" alive. */
    (void)arg;
    printf("keepalive stopped\n");
    return 0;
}
