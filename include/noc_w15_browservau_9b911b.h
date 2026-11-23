#ifndef NOC_W15_BROWSERVAU_9B911B_H
#define NOC_W15_BROWSERVAU_9B911B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_browservau_9b911b_request @ tools/browser_vault_preview.py:_request */
struct json_t *w15_browservau_9b911b_request(const struct json_t *raw);
/* PoP: w15_browservau_9b911b_unavailable @ tools/browser_vault_preview.py:_unavailable */
struct json_t *w15_browservau_9b911b_unavailable(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_BROWSERVAU_9B911B_H */
