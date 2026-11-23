#ifndef NOC_W15_BROWSERVAU_3F8D5C_H
#define NOC_W15_BROWSERVAU_3F8D5C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_browservau_3f8d5c_saved_origins @ tools/browser_vault_context.py:saved_origins */
struct json_t *w15_browservau_3f8d5c_saved_origins(const struct json_t *meta);
/* PoP: w15_browservau_3f8d5c_snapshot_refusal @ tools/browser_vault_context.py:snapshot_refusal */
struct json_t *w15_browservau_3f8d5c_snapshot_refusal(int custom_context, const struct json_t *fresh,
                                                     const char *meta_kind, const char *page_origin);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_BROWSERVAU_3F8D5C_H */
