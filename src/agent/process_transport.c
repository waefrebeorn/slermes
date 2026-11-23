/*
 * process_transport.c - real C11 port of upstream Hermes agent/process_transport.py.
 *
 * PoP: w9_processtran_ea08b8_is_local_server  @ agent/process_transport.py:is_local_server
 * PoP: w9_processtran_ea08b8_waits_like_local @ agent/process_transport.py:waits_like_local
 *
 * Both rules are keyed on auth_type, never on the URL.  The two predicates
 * (is_local_endpoint and _is_external_process_provider) are foreign modules;
 * the oracle seeds them from the fixture and C reproduces this module's own
 * boolean composition over the same inputs.
 */
#include "hermes_json.h"
#include "noc_w9_processtran_ea08b8.h"
#include <string.h>

static int truthy_url(const char *base_url) {
    return base_url && base_url[0] != '\0';   /* bool(base_url) */
}

bool w9_processtran_ea08b8_is_local_server(const char *base_url,
                                            bool local, bool external)
{
    /* bool(base_url) and is_local_endpoint(str(base_url)) and not external */
    return truthy_url(base_url) && local && !external;
}

bool w9_processtran_ea08b8_waits_like_local(const char *base_url,
                                             bool local, bool external)
{
    /* external or (bool(base_url) and is_local_endpoint(str(base_url))) */
    return external || (truthy_url(base_url) && local);
}
