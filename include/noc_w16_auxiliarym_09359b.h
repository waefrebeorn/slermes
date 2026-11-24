#ifndef NOC_W16_AUXILIARYM_09359B_H
#define NOC_W16_AUXILIARYM_09359B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_auxiliarym_09359b_netloc @ agent/auxiliary_managed_local.py:urlparse(base).netloc.lower() */
struct json_t *w16_auxiliarym_09359b_netloc(const struct json_t *base_url);
/* PoP: w16_auxiliarym_09359b_managed @ agent/auxiliary_managed_local.py:_is_managed_local_endpoint */
struct json_t *w16_auxiliarym_09359b_managed(const struct json_t *base_url,
                                              const struct json_t *managed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_AUXILIARYM_09359B_H */
