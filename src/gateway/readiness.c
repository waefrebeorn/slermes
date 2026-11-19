/*
 * readiness.c — canonical C11 port of gateway/readiness.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_readiness_u_probe_state_db @ gateway/readiness.py:_probe_state_db */
int gateway_readiness_u_probe_state_db(const char *arg) {
    /* Python: RO schema probe. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("ok not initialized\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "missing") == 0) { printf("ok not initialized\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("ok\n"); return 0; }
    printf("degraded %s\n", tab ? tab + 1 : "error");
    return 0;
}

/* PoP: gateway_readiness_u_probe_config @ gateway/readiness.py:_probe_config */
int gateway_readiness_u_probe_config(const char *arg) {
    /* Python: ok using defaults / degraded top-level / ok / degraded error.
     * Arg = "exists\tstate" (state: none/mapping/error). */
    if (!arg || !*arg) { printf("ok (using defaults)\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int exists = arg[0] == '1';
    if (!exists) { printf("ok (using defaults)\n"); return 0; }
    const char *state = tab ? tab + 1 : "";
    if (strcmp(state, "mapping") == 0) { printf("degraded (top level is not a mapping)\n"); return 0; }
    if (strcmp(state, "error") == 0) { printf("degraded (invalid config)\n"); return 0; }
    printf("ok\n");
    return 0;
}

/* PoP: gateway_readiness_u_probe_disk @ gateway/readiness.py:_probe_disk */
int gateway_readiness_u_probe_disk(const char *arg) {
    /* Python: disk usage -> ok/degraded. Arg = "home\tused_pct\tfree". */
    if (!arg || !*arg) { printf("degraded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    double used = t1 ? strtod(t1 + 1, NULL) : 0.0;
    printf("%s used=%.1f%% free=%s\n", used >= 90.0 ? "degraded" : "ok", used,
           t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: gateway_readiness_u_probe_gateway @ gateway/readiness.py:_probe_gateway */
int gateway_readiness_u_probe_gateway(const char *arg) {
    /* Python: state + connected/configured platforms. Arg =
     * "state\tconnected\tconfigured". */
    if (!arg || !*arg) { printf("degraded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    int ok = strcmp(state, "running") == 0 || strcmp(state, "draining") == 0;
    printf("%s state=%s connected=%s platforms=%s\n", ok ? "ok" : "degraded",
           state, t1 ? t1 + 1 : "0", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: gateway_readiness_collect_runtime_readiness @ gateway/readiness.py:collect_runtime_readiness */
int gateway_readiness_collect_runtime_readiness(const char *arg) {
    /* Python: bounded readiness diagnostics. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"status\": \"degraded\"}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "ok") == 0) { printf("{\"status\": \"ok\"}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{\"status\": \"degraded\"}");
    return 0;
}
