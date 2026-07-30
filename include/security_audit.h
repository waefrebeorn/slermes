#ifndef HERMES_SECURITY_AUDIT_H
#define HERMES_SECURITY_AUDIT_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void security_osv_severity_from_record(const json_t *record, char *out, size_t out_cap);
json_t *security_osv_fixed_versions(const json_t *record);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SECURITY_AUDIT_H */
