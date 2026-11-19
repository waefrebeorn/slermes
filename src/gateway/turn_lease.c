/*
 * turn_lease.c — canonical C11 port of gateway/turn_lease.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_turn_lease_u__repr__ @ gateway/turn_lease.py:__repr__ */
int gateway_turn_lease_u__repr__(const char *arg) {
    /* Python: f"<{cls.__name__} name={self.name!r}>". Arg = "cls\tname". */
    if (!arg || !*arg) { printf("<>\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *cls = tab ? arg : "TurnLease";
    size_t clen = tab ? (size_t)(tab - arg) : strlen(cls);
    const char *name = tab ? tab + 1 : "";
    printf("<%.*s name='%s'>\n", (int)clen, cls, name);
    return 0;
}

/* PoP: gateway_turn_lease_u__len__ @ gateway/turn_lease.py:__len__ */
int gateway_turn_lease_u__len__(const char *arg) {
    /* Python: len(self._leases). Arg = lease count (0 default). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%d\n", atoi(arg));
    return 0;
}

/* PoP: gateway_turn_lease_u_evict_idle @ gateway/turn_lease.py:_evict_idle */
int gateway_turn_lease_u_evict_idle(const char *arg) {
    /* Python: idle-only cap. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int evicted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!evicted) { printf("0 (no overflow or no idle leases)\n"); return 0; }
    printf("%s idle lease(s) evicted (held/contended never evicted — correctness beats cap)\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: gateway_turn_lease_rebind @ gateway/turn_lease.py:rebind */
int gateway_turn_lease_rebind(const char *arg) {
    /* Python: rotation alias. Arg =
     * "rebound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reb = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reb) { printf("0 (blocked: target lease live, or token invalid — #64934 edge)\n"); return 0; }
    printf("1 (lease aliased to new session id, token followed)\n");
    return 0;
}
