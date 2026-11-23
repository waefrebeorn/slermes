#ifndef NOC_W10_SESSIONREC_51B4DF_H
#define NOC_W10_SESSIONREC_51B4DF_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessionrec_51b4df_profile_from_session_key @ gateway/session_recovery.py:_profile_from_session_key
 * Mirrors session_keys.profile_from_session_key + profile_from_session_key_namespace.
 * Returns a newly allocated JSON string or JSON null. */
struct json_t *w10_sessionrec_51b4df_profile_from_session_key(const char *session_key);
/* PoP: w10_sessionrec_51b4df_recovered_row_allowed @ gateway/session_recovery.py:_recovered_row_allowed_for_active_profile
 * multiplex_profiles and active_profile are explicit seams for config/active-profile reads.
 * Returns a JSON bool. */
struct json_t *w10_sessionrec_51b4df_recovered_row_allowed(const char *requested_session_key,
                                                          const struct json_t *recovered,
                                                          int multiplex_profiles,
                                                          const char *active_profile);
/* PoP: w10_sessionrec_51b4df_recovered_row_matches_source_scope @ gateway/session_recovery.py:_recovered_row_matches_source_scope
 * source object keys: platform (str), chat_type (str), scope_id (str). Platform.SLACK
 * is the literal 'slack'. Returns a JSON bool. */
struct json_t *w10_sessionrec_51b4df_recovered_row_matches_source_scope(const struct json_t *recovered,
                                                                       const struct json_t *source);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONREC_51B4DF_H */
