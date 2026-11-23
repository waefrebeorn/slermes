#ifndef NOC_W15_METHODSDIS_DF8F9B_H
#define NOC_W15_METHODSDIS_DF8F9B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_methodsdis_df8f9b_mint_viewer_id @ tui_gateway/methods_display.py:_mint_viewer_id
 * Server-minted viewer identity: a requested id is honoured only when THIS connection
 * minted it earlier; otherwise a fresh id is minted and remembered. `mine` is the set
 * of ids minted for this connection, `generated` is the replacement token. */
struct json_t *w15_methodsdis_df8f9b_mint_viewer_id(const char *requested,
                                                     const struct json_t *mine,
                                                     const char *generated);

/* PoP: w15_methodsdis_df8f9b_foreign_viewer_id @ tui_gateway/methods_display.py:_foreign_viewer_id
 * True when `viewer_id` was NOT minted for this connection (Python returns an error dict
 * for foreign ids and None for its own). */
struct json_t *w15_methodsdis_df8f9b_foreign_viewer_id(const char *viewer_id,
                                                        const struct json_t *mine);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_METHODSDIS_DF8F9B_H */
