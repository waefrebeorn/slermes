#ifndef NOC_W14_SESSIONCOL_BF4EFD_H
#define NOC_W14_SESSIONCOL_BF4EFD_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessioncol_bf4efd_stored_transcript @ gateway/session_cold_review.py:stored_transcript
 * state is {session_id: str|null, history: [...]}. Returns
 * {session_id, history}: history is [] unless it has a user/assistant turn. */
struct json_t *w14_sessioncol_bf4efd_stored_transcript(const struct json_t *state);
/* PoP: w14_sessioncol_bf4efd_is_conversational @ gateway/session_cold_review.py:stored_transcript
 * True when the stored history has at least one user/assistant turn. */
struct json_t *w14_sessioncol_bf4efd_is_conversational(const struct json_t *state);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONCOL_BF4EFD_H */
