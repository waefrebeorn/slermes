#ifndef NOC_W8_IMAGETOKEN_C457D3_H
#define NOC_W8_IMAGETOKEN_C457D3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_imagetoken_c457d3_key @ agent/image_token_cost.py:_key
 * hosts maps base_url string -> hostname (injected base_url_hostname seam). */
struct json_t *w8_imagetoken_c457d3_key(const struct json_t *model,
                                        const struct json_t *base_url,
                                        const struct json_t *hosts);

/* PoP: w8_imagetoken_c457d3_learned @ agent/image_token_cost.py:learned_image_token_cost
 * table is the injected _table() result; default is DEFAULT_IMAGE_TOKEN_COST. */
struct json_t *w8_imagetoken_c457d3_learned(const struct json_t *model,
                                            const struct json_t *base_url,
                                            const struct json_t *hosts,
                                            const struct json_t *table);

/* PoP: w8_imagetoken_c457d3_current @ agent/image_token_cost.py:current_image_token_cost
 * bound is the injected ContextVar value (null -> default). */
struct json_t *w8_imagetoken_c457d3_current(const struct json_t *bound);

/* PoP: w8_imagetoken_c457d3_calibrate @ agent/image_token_cost.py:calibrate_from_usage
 * args: anchor, messages, prompt_tokens, text_only, hosts, model, base_url,
 * table. Collaborators count_images / anchored_context_tokens are injected. */
struct json_t *w8_imagetoken_c457d3_calibrate(const struct json_t *args);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_IMAGETOKEN_C457D3_H */
