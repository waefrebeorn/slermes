/*
 * port_service_manager.h -- pure helpers ported from hermes_cli/service_manager.py.
 *
 * The module's subprocess / unit-file / chown half stays in Python; only the
 * profile-name contract and the service-name mapping are here.
 */
#ifndef PORT_SERVICE_MANAGER_H
#define PORT_SERVICE_MANAGER_H

#include <stddef.h>

/* service_manager.py:validate_profile_name -- 0 when the name is
 * filesystem/s6-safe, -1 otherwise with the Python message in err (when
 * non-NULL). */
int sm_validate_profile_name(const char *name, char *err, size_t errlen);

/* service_manager.py:_profile_from_service -- strip the "gateway-" prefix
 * (removeprefix semantics: no-op when absent). Caller frees. */
char *sm_profile_from_service(const char *name);

#endif /* PORT_SERVICE_MANAGER_H */
