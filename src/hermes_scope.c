/*
 * hermes_scope.c — real C11 port of the pure leaves of hermes_scope.py.
 *
 * PoP: hermesscope_principal_ok @ hermes_scope.py:Principal.__post_init__
 * PoP: hermesscope_system_ok    @ hermes_scope.py:System.__post_init__
 * PoP: hermesscope_group_ok     @ hermes_scope.py:Group.__post_init__
 * PoP: hermesscope_fence_ok     @ hermes_scope.py:_positive_int
 * PoP: hermesscope_is_local     @ hermes_scope.py:Scope.is_local
 * PoP: hermesscope_home         @ hermes_scope.py:Scope.home
 *
 * The module's context-local state machine (push_state/pop_state/_fold) is
 * token/weakref driven and out of scope for a JSON leaf oracle. These six
 * exports are the deterministic validation/derivation leaves.
 */
#include "noc_w5_hermesscope_980e36.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A value passes the value-object guards iff it is a non-empty JSON string:
 * `isinstance(x, str) and x` in the Python source. Numbers, bools and null
 * are distinct json types and therefore rejected, matching Python's
 * `isinstance(..., str)`. */
static int hs_nonempty_str(json_t *v) {
    return v && v->type == JSON_STRING && v->str_val && v->str_val[0] != '\0';
}

/*
 * PoP: hermesscope_principal_ok @ hermes_scope.py:Principal.__post_init__
 */
int hermesscope_principal_ok(json_t *v) { return hs_nonempty_str(v); }

/*
 * PoP: hermesscope_system_ok @ hermes_scope.py:System.__post_init__
 */
int hermesscope_system_ok(json_t *v) { return hs_nonempty_str(v); }

/*
 * PoP: hermesscope_group_ok @ hermes_scope.py:Group.__post_init__
 */
int hermesscope_group_ok(json_t *v) { return hs_nonempty_str(v); }

/*
 * PoP: hermesscope_fence_ok @ hermes_scope.py:_positive_int
 */
int hermesscope_fence_ok(json_t *epoch, json_t *generation) {
    /* _positive_int: `type(value) is int and value >= 1`. A JSON bool is a
     * distinct type (JSON_BOOL) and is refused, as is any non-integral
     * number (Python float fails `type(value) is int`). */
    if (!epoch || epoch->type != JSON_NUMBER) return 0;
    if (!generation || generation->type != JSON_NUMBER) return 0;
    if (!isfinite(epoch->num_val) || !isfinite(generation->num_val)) return 0;
    if (epoch->num_val < 1 || generation->num_val < 1) return 0;
    if (floor(epoch->num_val) != epoch->num_val) return 0;
    if (floor(generation->num_val) != generation->num_val) return 0;
    return 1;
}

/*
 * PoP: hermesscope_is_local @ hermes_scope.py:Scope.is_local
 */
int hermesscope_is_local(const char *tenant) {
    return tenant && !strcmp(tenant, "local");
}

/*
 * PoP: hermesscope_home @ hermes_scope.py:Scope.home
 */
char *hermesscope_home(const char *tenant, const char *agent) {
    if (!hermesscope_is_local(tenant)) return NULL;
    const char *a = agent ? agent : "";
    size_t n = strlen(a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n + 1);
    return out;
}
