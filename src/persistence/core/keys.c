/*
 * keys.c - Profile a gateway session key names (C11 port).
 *
 * PoP: sesskeys_profile_from_session_key_namespace @ persistence/core/keys.py:profile_from_session_key_namespace
 * PoP: sesskeys_session_key_profile @ persistence/core/keys.py:session_key_profile
 *
 * Both entries return a malloc'd string (caller frees) or NULL for Python None.
 * The ``agent:<ns>:...`` split mirrors str.split(":") exactly, including the
 * empty-namespace and short-key cases.
 */
#include "noc_sesskeys.h"

#include <stdlib.h>
#include <string.h>

char *sesskeys_profile_from_session_key_namespace(const char *ns) {
    if (!ns) return NULL;
    if (!strcmp(ns, "main")) return strdup("default");
    if (!strcmp(ns, "main~")) return strdup("main");
    return strdup(ns);
}

char *sesskeys_session_key_profile(const char *session_key) {
    if (!session_key) return NULL; /* non-str -> None */
    const char *c0 = strchr(session_key, ':');
    if (!c0) return NULL;                 /* fewer than 3 parts */
    const char *p1 = c0 + 1;
    const char *c1 = strchr(p1, ':');
    if (!c1) return NULL;                 /* fewer than 3 parts */
    if ((size_t)(c0 - session_key) != 5 || strncmp(session_key, "agent", 5)) return NULL;
    size_t l1 = (size_t)(c1 - p1);
    if (l1 == 0) return NULL;             /* empty namespace slot */
    char buf[512];
    if (l1 >= sizeof(buf)) return NULL;
    memcpy(buf, p1, l1); buf[l1] = '\0';
    if (!strcmp(buf, "main")) return strdup("default");
    if (!strcmp(buf, "main~")) return strdup("main");
    return strdup(buf);
}
