#ifndef NOC_W12_SESSIONANC_9A64BB_H
#define NOC_W12_SESSIONANC_9A64BB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_sessionanc_9a64bb_subagent_row @ gateway/session_ancillary.py:subagent_row
 * One subagent.list row: the 11 _SUBAGENT_FIELDS plus child_session_id (a non-empty str only). */
struct json_t *w12_sessionanc_9a64bb_subagent_row(const struct json_t *record);
/* PoP: w12_sessionanc_9a64bb_reachable_upload_home @ gateway/session_ancillary.py:_reachable_upload_home
 * A staged upload's source home is the authority's own or, host-scoped, a served sibling.
 * Absolute paths only; seam inputs replace the connection object. */
struct json_t *w12_sessionanc_9a64bb_reachable_upload_home(const struct json_t *authority_home,
                                                           const struct json_t *host_scoped,
                                                           const struct json_t *siblings,
                                                           const struct json_t *raw_home);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_SESSIONANC_9A64BB_H */
