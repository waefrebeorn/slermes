#ifndef NOC_W16_SESSIONLIV_6D3242_H
#define NOC_W16_SESSIONLIV_6D3242_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionliv_6d3242_execution_state @ gateway/session_live_listing.py:_execution_state */
struct json_t *w16_sessionliv_6d3242_execution_state(const struct json_t *pending);
/* PoP: w16_sessionliv_6d3242_masked_pending @ gateway/session_live_listing.py:_masked_pending */
struct json_t *w16_sessionliv_6d3242_masked_pending(const struct json_t *rows,
                                                    const struct json_t *settling);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONLIV_6D3242_H */
