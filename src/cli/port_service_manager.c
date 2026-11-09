/*
 * port_service_manager.c
 *
 * Pure helpers ported from hermes_cli/service_manager.py - systemd/s6 service
 * management for the gateway.
 *
 * Almost the whole module is subprocess and filesystem work (enable/disable,
 * start/stop, unit generation, chown). The two portable pieces are the profile
 * name contract and the service-name mapping:
 *
 *   sm_validate_profile_name <- service_manager.py:validate_profile_name
 *   sm_profile_from_service  <- service_manager.py:_profile_from_service
 *
 * validate_profile_name RAISES in Python; the C form returns 0 on success and
 * writes the raised message into err (when non-NULL), which is the same
 * contract the other error-copying ports in this tree use.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* service_manager.py:_MAX_PROFILE_LEN - s6-svscan's default name_max */
#define SM_MAX_PROFILE_LEN 251

/* service_manager.py:S6_SERVICE_PREFIX */
#define SM_S6_SERVICE_PREFIX "gateway-"

/* ===========================================================================
 *  validate_profile_name  (service_manager.py:validate_profile_name)
 *
 *      if not name:            raise ValueError("profile name must not be empty")
 *      if len(name) > 251:     raise ValueError("profile name too long (...)")
 *      if not ^[a-z0-9][a-z0-9_-]*$:
 *                              raise ValueError("profile name must match ...")
 *
 *  Returns 0 when valid, -1 when not, with the Python message text in err.
 * =========================================================================== */

/* PoP: validate_profile_name @ hermes_cli/service_manager.py:validate_profile_name */
int sm_validate_profile_name(const char *name, char *err, size_t errlen)
{
    if (err && errlen) err[0] = '\0';

    if (!name || !name[0]) {
        if (err && errlen) snprintf(err, errlen, "profile name must not be empty");
        return -1;
    }
    size_t len = strlen(name);
    if (len > SM_MAX_PROFILE_LEN) {
        if (err && errlen)
            snprintf(err, errlen, "profile name too long (%zu > %d)",
                     len, SM_MAX_PROFILE_LEN);
        return -1;
    }
    /* ^[a-z0-9][a-z0-9_-]*$ */
    if (!((name[0] >= 'a' && name[0] <= 'z') ||
          (name[0] >= '0' && name[0] <= '9'))) {
        if (err && errlen)
            snprintf(err, errlen,
                     "profile name must match [a-z0-9][a-z0-9_-]*, got '%s'", name);
        return -1;
    }
    for (size_t i = 1; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            if (err && errlen)
                snprintf(err, errlen,
                         "profile name must match [a-z0-9][a-z0-9_-]*, got '%s'", name);
            return -1;
        }
    }
    return 0;
}

/* ===========================================================================
 *  _profile_from_service  (service_manager.py:_profile_from_service)
 *
 *      name.removeprefix("gateway-")
 *
 *  Returns a NEW string the caller frees. removeprefix does NOTHING when the
 *  prefix is absent (unlike a naive strip), so "gateway-" alone yields "".
 * =========================================================================== */

/* PoP: _profile_from_service @ hermes_cli/service_manager.py:_profile_from_service */
char *sm_profile_from_service(const char *name)
{
    const char *in = name ? name : "";
    size_t plen = strlen(SM_S6_SERVICE_PREFIX);
    const char *body = in;
    if (strncmp(in, SM_S6_SERVICE_PREFIX, plen) == 0) body = in + plen;

    char *out = (char *)malloc(strlen(body) + 1);
    if (!out) return NULL;
    strcpy(out, body);
    return out;
}
