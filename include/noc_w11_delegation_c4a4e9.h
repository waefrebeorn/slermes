#ifndef NOC_W11_DELEGATION_C4A4E9_H
#define NOC_W11_DELEGATION_C4A4E9_H
#include "hermes_json.h"

/* PoP port of agent/delegation_registry.py (gate w11_delegation_c4a4e9). */
char *w11_delegation_c4a4e9_configured_backend(json_t *config);
char *w11_delegation_c4a4e9_lower_key(const char *name);
int   w11_delegation_c4a4e9_bundled_name_matches(const char *name);
#endif /* NOC_W11_DELEGATION_C4A4E9_H */
