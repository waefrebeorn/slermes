#ifndef SRC_TOOLS_WEB_BASE64_IMG_H
#define SRC_TOOLS_WEB_BASE64_IMG_H

/*
 * web_base64_img.h — Port of Python: tools/web_tools.py:convert_base64_images_to_links
 *
 * Replaces inline base64 image blobs with labeled markdown placeholders so the
 * model never receives tens of thousands of base64 characters inline. Real
 * (http/https) markdown image links are left untouched.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convert inline base64 image payloads in `text` into inspectable
 * placeholders. Caller owns the returned string (free() it).
 *   ![alt](data:image/png;base64,AAAA...) -> [IMAGE: alt]  (or [IMAGE])
 *   (data:image/png;base64,AAAA...)       -> [IMAGE]
 *   bare data:image/...;base64,AAAA...         -> [IMAGE]
 * Returns NULL on allocation failure, a strdup'd copy otherwise.
 */
char *web_base64_img_convert(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* SRC_TOOLS_WEB_BASE64_IMG_H */
