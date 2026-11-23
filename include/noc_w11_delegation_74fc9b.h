#ifndef NOC_W11_DELEGATION_74FC9B_H
#define NOC_W11_DELEGATION_74FC9B_H
#include "hermes_json.h"

/* PoP port of agent/delegation_backends.py (gate w11_delegation_74fc9b). */
json_t *w11_delegation_74fc9b_route_keys(json_t *provider);
json_t *w11_delegation_74fc9b_delegate_def(json_t *tools);
char   *w11_delegation_74fc9b_pinned_backend(json_t *agent);
#endif /* NOC_W11_DELEGATION_74FC9B_H */
