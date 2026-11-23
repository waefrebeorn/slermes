#ifndef NOC_W10_SESSIONIDE_E2126E_H
#define NOC_W10_SESSIONIDE_E2126E_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessionide_e2126e_name @ gateway/session_identity.py:_name
 * str.strip() of a string value, or JSON null. */
struct json_t *w10_sessionide_e2126e_name(const struct json_t *value);
/* PoP: w10_sessionide_e2126e_parse_authenticated_subject @ gateway/session_identity.py:parse_authenticated_subject
 * subject may be NULL; returns a 3-string JSON array or JSON null. */
struct json_t *w10_sessionide_e2126e_parse_authenticated_subject(const char *subject);
/* PoP: w10_sessionide_e2126e_authenticated_subject @ gateway/session_identity.py:authenticated_subject
 * identity is a JSON object; returns a newly allocated JSON string. */
struct json_t *w10_sessionide_e2126e_authenticated_subject(const struct json_t *identity);
/* PoP: w10_sessionide_e2126e_is_authenticated_identity @ gateway/session_identity.py:is_authenticated_identity
 * INTERNAL_USER_ID / INTERNAL_PROVIDER are the literal 'server-internal' constants
 * imported from hermes_cli.dashboard_auth.ws_tickets. Returns a JSON bool. */
struct json_t *w10_sessionide_e2126e_is_authenticated_identity(const struct json_t *identity);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONIDE_E2126E_H */
