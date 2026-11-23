#ifndef NOC_SESSION_SKIN_H
#define NOC_SESSION_SKIN_H
struct json_t;
/* Resolved skin dict for the owning profile (empty dict on failure).
 * Caller owns the returned JSON node. */
struct json_t *session_skin_resolved_skin(void);
#endif
