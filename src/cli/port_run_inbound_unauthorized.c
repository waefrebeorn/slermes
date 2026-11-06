/*
 * port_run_inbound_unauthorized.c — Port of
 * gateway/run_inbound_unauthorized.py::pairing_profile_arg: the pure mapping
 * from a pairing store's profile string to the ``-p <profile> `` CLI arg (or
 * ""). Other stubs in this module route through the i18n catalog (t()) and are
 * left to the runtime half.
 */

#include "port_run_inbound_unauthorized.h"

#include <stdlib.h>
#include <string.h>

/* PoP: pairing_profile_arg @ gateway/run_inbound_unauthorized.py:pairing_profile_arg
 * Python: store_profile = getattr(pairing_store, "profile", None); if str,
 * non-empty and != "default" -> "-p <profile> ", else "". profile==NULL => "". */
char *run_inbound_pairing_profile_arg(const char *profile)
{
    const char *p = profile ? profile : "";
    if (!*p || strcmp(p, "default") == 0)
        return strdup("");

    size_t n = strlen(p);
    char *out = malloc(n + 4); /* "-p " + profile + " " + NUL */
    if (!out)
        return NULL;
    out[0] = '-';
    out[1] = 'p';
    out[2] = ' ';
    memcpy(out + 3, p, n);
    out[n + 3] = ' ';
    out[n + 4] = '\0';
    return out;
}
