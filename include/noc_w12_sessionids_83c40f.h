#ifndef NOC_W12_SESSIONIDS_83C40F_H
#define NOC_W12_SESSIONIDS_83C40F_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionids_83c40f_is_known_session_id @ session_rules/session_ids.py:is_known_session_id
 * Faithful port of all 8 SESSION_ID_RECOGNIZERS (re.match, stream anchors). */
int w12_sessionids_83c40f_is_known_session_id(const char *value);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONIDS_83C40F_H */
