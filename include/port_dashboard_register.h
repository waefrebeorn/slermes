/*
 * port_dashboard_register.h -- pure URL helpers ported from
 * hermes_cli/dashboard_register.py.
 *
 * The urllib calls, .env persistence and random name generation stay in Python.
 */
#ifndef PORT_DASHBOARD_REGISTER_H
#define PORT_DASHBOARD_REGISTER_H

#include <stddef.h>

/* dashboard_register.py:_public_url_from_redirect -- the origin
 * (scheme://host[:port]) of a redirect URI, or "" when it is not an
 * http(s) URL. Caller frees. */
char *dr_public_url_from_redirect(const char *redirect_uri);

/* dashboard_register.py:_resolve_portal_base_url -- explicit override, then
 * the stored auth-state base, then the production default; trailing slashes
 * stripped. stored_base/default_base stand in for the credential lookup.
 * Caller frees. */
char *dr_resolve_portal_base_url(const char *override,
                                 const char *stored_base,
                                 const char *default_base);

#endif /* PORT_DASHBOARD_REGISTER_H */
