#ifndef NOC_IMGCATALOG_H
#define NOC_IMGCATALOG_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: imgcatalog_default_model @ tools/image_generation_catalog.py:DEFAULT_MODEL */
char *imgcatalog_default_model(void);
/* PoP: imgcatalog_default_aspect_ratio @ tools/image_generation_catalog.py:DEFAULT_ASPECT_RATIO */
char *imgcatalog_default_aspect_ratio(void);
/* PoP: imgcatalog_valid_aspect_ratios @ tools/image_generation_catalog.py:VALID_ASPECT_RATIOS */
struct json_t *imgcatalog_valid_aspect_ratios(void);
/* PoP: imgcatalog_upscaler @ tools/image_generation_catalog.py:UPSCALER_MODEL */
struct json_t *imgcatalog_upscaler(void);
/* PoP: imgcatalog_model_field @ tools/image_generation_catalog.py:FAL_MODELS */
struct json_t *imgcatalog_model_field(const char *key, const char *field);
/* PoP: imgcatalog_model_keys @ tools/image_generation_catalog.py:FAL_MODELS */
struct json_t *imgcatalog_model_keys(void);
/* PoP: imgcatalog_build_entry @ tools/image_generation_catalog.py:_model */
struct json_t *imgcatalog_build_entry(const struct json_t *params);

#ifdef __cplusplus
}
#endif

#endif /* NOC_IMGCATALOG_H */
