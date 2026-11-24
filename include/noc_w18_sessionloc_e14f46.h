#ifndef NOC_W18_SESSIONLOC_E14F46_H
#define NOC_W18_SESSIONLOC_E14F46_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionloc_e14f46_merge_live_in_stored_order @ gateway/session_local_recovery.py:_merge_live_in_stored_order
 *
 * The live rows in stored (_row_id) order, each archived display row placed
 * before the live row it preceded in display order. Archived rows are copies
 * of the display rows with "archived": true added. Returns a new array
 * (caller frees).
 */
struct json_t *w18_sessionloc_e14f46_merge_live_in_stored_order(
    const struct json_t *display, const struct json_t *live);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONLOC_E14F46_H */
