#ifndef NOC_W16_AUXILIARYA_3C2054_H
#define NOC_W16_AUXILIARYA_3C2054_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_auxiliarya_3c2054_async_key @ agent/auxiliary_async_rebuild.py:async_api_key */
struct json_t *w16_auxiliarya_3c2054_async_key(const struct json_t *client);
/* PoP: w16_auxiliarya_3c2054_headers @ agent/auxiliary_async_rebuild.py:configured_default_headers */
struct json_t *w16_auxiliarya_3c2054_headers(const struct json_t *client);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_AUXILIARYA_3C2054_H */
