#ifndef NOC_W16_SESSIONSTO_8C91AF_H
#define NOC_W16_SESSIONSTO_8C91AF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionsto_8c91af_take @ gateway/session_stored_reads.py:_take */
struct json_t *w16_sessionsto_8c91af_take(const struct json_t *roles, int latest, long long limit);
/* PoP: w16_sessionsto_8c91af_cut @ gateway/session_stored_reads.py:_cut */
struct json_t *w16_sessionsto_8c91af_cut(const struct json_t *roles, int latest, long long taken);
/* PoP: w16_sessionsto_8c91af_sent @ gateway/session_stored_reads.py:_sent */
struct json_t *w16_sessionsto_8c91af_sent(const struct json_t *handoffs, int latest,
                                          const struct json_t *item);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONSTO_8C91AF_H */
