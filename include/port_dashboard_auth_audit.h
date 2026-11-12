#ifndef SLERMES_PORT_DASHBOARD_AUTH_AUDIT_H
#define SLERMES_PORT_DASHBOARD_AUTH_AUDIT_H

#include "hermes_json.h"

/* _bounded_value(value, max_len, marker): recursively copy a JSON-shaped value,
 * truncating every string longer than max_len to (max_len - strlen(marker))
 * chars plus marker. Mirrors the live audit-log field cap. Caller frees. */
json_t *dab_bounded_value(const json_t *value, int max_len, const char *marker);

#endif /* SLERMES_PORT_DASHBOARD_AUTH_AUDIT_H */
