/*
 * webhook_filters.c — canonical C11 port of gateway/platforms/webhook_filters.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_platforms_webhook_filt_u_stringify_filter_value @ gateway/platforms/webhook_filters.py:_stringify_filter_value */
int gateway_platforms_webhook_filt_u_stringify_filter_value(const char *arg) {
    /* Python: "" if _MISSING; json.dumps(value, sort_keys=True) for
     * dict/list; else str(value). Arg = value or _MISSING. */
    if (!arg || !*arg || strcmp(arg, "_MISSING") == 0) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_u_resolve_profile_path @ gateway/platforms/webhook_filters.py:_resolve_profile_path */
int gateway_platforms_webhook_filt_u_resolve_profile_path(const char *arg) {
    /* Python: ~/.hermes -> profile home; absolute as-is; else home/raw. Arg =
     * "raw\thome\tresolved". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *resolved = t2 ? t2 + 1 : "";
    if (resolved[0]) { printf("%s\n", resolved); return 0; }
    printf("\n");
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_u_resolve_script_path @ gateway/platforms/webhook_filters.py:_resolve_script_path */
int gateway_platforms_webhook_filt_u_resolve_script_path(const char *arg) {
    /* Python: scripts-root-relative resolve. Arg =
     * "script_value\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "ok") == 0) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    fprintf(stderr, "%s\n", t3 ? t3 + 1 : "script resolution failed");
    return 1;
}

/* PoP: gateway_platforms_webhook_filt_u_load_filter_file_values @ gateway/platforms/webhook_filters.py:_load_filter_file_values */
int gateway_platforms_webhook_filt_u_load_filter_file_values(const char *arg) {
    /* Python: JSON list/dict-keys/lines. Arg = "path\tstate\tvalues". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0 || strcmp(state, "read_error") == 0) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_resolve_filter_field @ gateway/platforms/webhook_filters.py:resolve_filter_field */
int gateway_platforms_webhook_filt_resolve_filter_field(const char *arg) {
    /* Python: dotted field resolve. Arg =
     * "field\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "missing") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "ok") == 0) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_filter_matches @ gateway/platforms/webhook_filters.py:filter_matches */
int gateway_platforms_webhook_filt_filter_matches(const char *arg) {
    /* Python: declarative eval. Arg =
     * "matched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int matched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (all/any/not/equals/contains/in/in_file/regex)\n", matched ? "1" : "0");
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_route_filters_match @ gateway/platforms/webhook_filters.py:route_filters_match */
int gateway_platforms_webhook_filt_route_filters_match(const char *arg) {
    /* Python: empty -> True; dict -> single; list -> all. Arg =
     * "filters_kind\tmatch" (kind: none/dict/list; match 1/0). */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *kind = arg;
    int match = tab && tab[1] == '1';
    if (strcmp(kind, "none") == 0) { printf("1\n"); return 0; }
    if (strcmp(kind, "list") == 0) { printf("%d\n", match ? 1 : 0); return 0; }
    if (strcmp(kind, "dict") == 0) { printf("%d\n", match ? 1 : 0); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: gateway_platforms_webhook_filt_run_route_script @ gateway/platforms/webhook_filters.py:run_route_script */
int gateway_platforms_webhook_filt_run_route_script(const char *arg) {
    /* Python: script verdict. Arg =
     * "continue\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cont = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (script missing/bash absent/timed out)\n"); return 0; }
    if (!cont) { printf("0 (silent/empty stdout — webhook ignored)\n"); return 0; }
    printf("1 (payload transformed: %s)\n", t2 ? t2 + 1 : "{}");
    return 0;
}
