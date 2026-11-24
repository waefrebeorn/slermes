#ifndef NOC_W18_LEFTCOREM_324DAB_H
#define NOC_W18_LEFTCOREM_324DAB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: leftcorem_first_cause @ hermes_cli/left_core_migration.py:_first_cause
 * One-line cause: git's fatal:/error: line else first non-empty line. Caller frees. */
char *leftcorem_first_cause(const char *error);

/* PoP: leftcorem_delegation_backend_routes @ hermes_cli/left_core_migration.py:_delegation_backend_routes
 * All dict options rows from every delegation.backends block. Caller frees the JSON. */
struct json_t *leftcorem_delegation_backend_routes(const struct json_t *delegation);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_LEFTCOREM_324DAB_H */
