/* gate: gateway_authz_mixin_adapter_profile_for_source
 * Faithful traced copy of src/gateway/ports_wrappers.c::
 * gateway_authz_mixin_u_adapter_profile_for_source (that translation unit
 * pulls the whole gateway surface, so the port body is inlined verbatim;
 * libjson not even needed — pure string logic).
 * LIVE twin: gateway/authz_mixin.py::_adapter_profile_for_source, driven
 * over real mixin/source objects with identity semantics.
 *
 * Input argv: "<matched>\t<owning_profile>\t<source_profile>".
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: adapter resolved (non-empty arg)
 *   B2: adapter is NOT the transport-registered one (policy lookup proceeds)
 *   VSTR(resolved profile, "" == None)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* verbatim port logic (returns malloc'd profile or "") */
static char *adapter_profile_for_source_port(const char *arg) {
    char *out = strdup("");
    if (!arg || !*arg) return out;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    free(out);
    if (arg[0] == '1') {
        if (!t1) return strdup("");
        return strndup(t1 + 1, (size_t)((t2 ? t2 : arg + strlen(arg)) - (t1 + 1)));
    }
    if (!t2) return strdup("");
    return strndup(t2 + 1, (size_t)(arg + strlen(arg) - (t2 + 1)));
}

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *arg0) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", arg0);
    decode_escapes(buf);
    const char *arg = buf;
    WCUP_BEGIN("_adapter_profile_for_source");
    WCUP_BRANCH(arg && *arg);                                   /* B1 */
    bool matched = arg && *arg && arg[0] == '1';
    WCUP_BRANCH(!matched);                                      /* B2 */
    char *r = adapter_profile_for_source_port(arg);
    WCUP_VALSTR(r);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    WCUP_END("_adapter_profile_for_source");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <matched\\tprof\\tsrc...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
