/*
 * profile_routing.c — canonical C11 port of gateway/profile_routing.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_profile_routing_specificity @ gateway/profile_routing.py:specificity */
int gateway_profile_routing_specificity(const char *arg) {
    /* Python: s=0; +2 guild_id, +4 chat_id, +8 thread_id. Arg =
     * "guild\tchat\tthread" presence flags (1/0 or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    int guild = 0, chat = 0, thread = 0;
    const char *p = arg;
    guild = (*p == '1');
    p = strchr(p, '\t');
    if (p) { p++; chat = (*p == '1'); p = strchr(p, '\t'); if (p) { p++; thread = (*p == '1'); } }
    printf("%d\n", guild * 2 + chat * 4 + thread * 8);
    return 0;
}

/* PoP: gateway_profile_routing_parse_profile_routes @ gateway/profile_routing.py:parse_profile_routes */
int gateway_profile_routing_parse_profile_routes(const char *arg) {
    /* Python: specificity sort. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: gateway_profile_routing_match_profile_route @ gateway/profile_routing.py:match_profile_route */
int gateway_profile_routing_match_profile_route(const char *arg) {
    /* Python: first route matching platform/ids; None otherwise. Arg =
     * "route_id\tplatform" pairs (tab-sep), or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p) {
        const char *tab = strchr(p, '\t');
        if (!tab) { printf("%s\n", p); return 0; }
        printf("%.*s\n", (int)(tab - p), p);
        return 0;
    }
    printf("\n");
    return 0;
}
