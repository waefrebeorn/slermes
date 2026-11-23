#ifndef NOC_W7_SECURITYPAC_7EAC2A_H
#define NOC_W7_SECURITYPAC_7EAC2A_H

#include "hermes_json.h"

/* Public C API for the pure URL-construction logic of pm/security_packages.py
 * (gate w7_securitypac_7eac2a). The secpack_ prefix avoids collisions with the
 * other package_* symbols in this repo. Returned json_t nodes are owned by the
 * caller (json_free). An unknown target returns JSON null, mirroring the
 * KeyError the live mapping would raise (the oracle skips those cases). */

struct json_t *secpack_bws_fetch_url(const char *version, const char *target);
struct json_t *secpack_iron_fetch_url(const char *version, const char *target);
struct json_t *secpack_iron_fetch_urls(const char *version, const char *target);

#endif /* NOC_W7_SECURITYPAC_7EAC2A_H */
