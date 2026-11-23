#ifndef NOC_W12_SESSIONAPI_CD0976_H
#define NOC_W12_SESSIONAPI_CD0976_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_sessionapi_cd0976_image_urls @ gateway/session_api_media.py:_image_urls
 * Every inline image_url part's url (missing/falsy image_url yields ""). */
struct json_t *w12_sessionapi_cd0976_image_urls(const struct json_t *content);
/* PoP: w12_sessionapi_cd0976_restore_images @ gateway/session_api_media.py:restore_api_images
 * Append the committed-reference / remote-url hints to the first text part.
 * media_paths is the restore_native_media(media) seam (paths in data-url order). */
struct json_t *w12_sessionapi_cd0976_restore_images(const struct json_t *content,
                                                     const struct json_t *media_paths);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_SESSIONAPI_CD0976_H */
